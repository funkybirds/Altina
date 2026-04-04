
#include "Rendering/BasicDeferredRenderer.h"

#include "Rendering/DrawListExecutor.h"
#include "Rendering/GraphicsPipelinePreset.h"
#include "Rendering/AmbientOcclusion/DeferredSsaoPassSet.h"
#include "Rendering/Shadowing/DeferredCsmPassSet.h"
#include "Deferred/DeferredTypes.h"
#include "Deferred/DeferredScenePasses.h"
#include "Deferred/DeferredCsm.h"

#include "FrameGraph/FrameGraph.h"
#include "Geometry/StaticMeshVertexFactory.h"
#include "Geometry/VertexLayoutBuilder.h"
#include "Lighting/LightTypes.h"
#include "Material/MaterialPass.h"
#include "Shadow/CascadedShadowMapping.h"
#include "Shader/ShaderBindingUtility.h"
#include "View/ViewData.h"

#include "Rendering/PostProcess/PostProcess.h"
#include "Rendering/PostProcess/PostProcessSettings.h"
#include "Rendering/RenderingSettings.h"

#include "Container/HashMap.h"
#include "Container/SmartPtr.h"
#include "Container/Vector.h"
#include "Logging/Log.h"
#include "Platform/Generic/GenericPlatformDecl.h"
#include "Rhi/Command/RhiCmdContext.h"
#include "Rhi/RhiBindGroup.h"
#include "Rhi/RhiBuffer.h"
#include "Rhi/RhiDebugMarker.h"
#include "Rhi/RhiDevice.h"
#include "Rhi/RhiInit.h"
#include "Rhi/RhiPipeline.h"
#include "Rhi/RhiPipelineLayout.h"
#include "Rhi/RhiTexture.h"
#include "Types/NumericProperties.h"

#include "Math/Common.h"
#include "Math/LinAlg/Common.h"
#include "Math/LinAlg/RenderingMath.h"
#include "Algorithm/Sort.h"
#include "Utility/Assert.h"

using AltinaEngine::Move;
using AltinaEngine::Core::Math::FMatrix4x4f;
using AltinaEngine::Core::Math::FVector3f;
using AltinaEngine::Core::Math::FVector4f;

using AltinaEngine::Core::Utility::Assert;
using AltinaEngine::Core::Utility::DebugAssert;

using AltinaEngine::Core::Container::FString;

namespace AltinaEngine::Rendering {
    namespace {
        namespace Container = Core::Container;
        using Container::FStringView;
        using Container::THashMap;
        using Container::TVector;
        using RenderCore::EMaterialPass;
        constexpr auto kFrameTimingCategory = TEXT("FrameTiming");

        // 0=PBR, 1=Lambert(debug). Written into the DeferredLighting cbuffer.
        u32            gDeferredLightingDebugShadingMode = 0U;
        using Deferred::FIblConstants;
        using Deferred::FInstanceDrawData;
        using Deferred::FPerFrameConstants;

        struct FBasePassPipelineStats {
            u32 mBaseHits             = 0U;
            u32 mBaseMisses           = 0U;
            u32 mShadowHits           = 0U;
            u32 mShadowMisses         = 0U;
            u32 mMaterialLayoutMisses = 0U;
            u32 mPipelineLayoutMisses = 0U;
        };

        thread_local FBasePassPipelineStats gBasePassPipelineStats{};

        void ResetBasePassPipelineStats() noexcept { gBasePassPipelineStats = {}; }

        struct FWorldBoundsDebug {
            bool      bValid        = false;
            FVector3f MinWS         = FVector3f(0.0f);
            FVector3f MaxWS         = FVector3f(0.0f);
            u32       BatchCount    = 0U;
            u32       InstanceCount = 0U;
        };

        [[nodiscard]] auto TransformAabbToWorld(const FMatrix4x4f& world,
            const RenderCore::Geometry::FStaticMeshBounds3f& localBounds, FVector3f& outMinWS,
            FVector3f& outMaxWS) -> bool {
            if (!localBounds.IsValid()) {
                return false;
            }
            return Core::Math::LinAlg::TransformAabbToWorld(
                world, localBounds.Max, localBounds.Min, outMinWS, outMaxWS);
        }

        [[nodiscard]] auto ComputeDrawListWorldBounds(const RenderCore::Render::FDrawList& list)
            -> FWorldBoundsDebug {
            FWorldBoundsDebug out{};
            out.BatchCount = list.GetBatchCount();

            FVector3f minWS(TNumericProperty<f32>::Max);
            FVector3f maxWS(-TNumericProperty<f32>::Max);

            list.ForEachBatch([&](const auto& batch) {
                const auto* mesh = batch.mStatic.mMesh;
                if (mesh == nullptr) {
                    return;
                }
                if (batch.mStatic.mLodIndex >= mesh->mLods.Size()) {
                    return;
                }

                const auto& lodBounds = mesh->mLods[batch.mStatic.mLodIndex].mBounds;
                if (!lodBounds.IsValid()) {
                    return;
                }

                for (const auto& inst : batch.mInstances) {
                    FVector3f instMinWS(0.0f);
                    FVector3f instMaxWS(0.0f);
                    if (!TransformAabbToWorld(inst.mWorld, lodBounds, instMinWS, instMaxWS)) {
                        continue;
                    }

                    minWS[0] = Core::Math::Min(minWS[0], instMinWS[0]);
                    minWS[1] = Core::Math::Min(minWS[1], instMinWS[1]);
                    minWS[2] = Core::Math::Min(minWS[2], instMinWS[2]);
                    maxWS[0] = Core::Math::Max(maxWS[0], instMaxWS[0]);
                    maxWS[1] = Core::Math::Max(maxWS[1], instMaxWS[1]);
                    maxWS[2] = Core::Math::Max(maxWS[2], instMaxWS[2]);
                    out.InstanceCount += 1U;
                }
            });

            out.bValid = (minWS[0] <= maxWS[0]) && (minWS[1] <= maxWS[1]) && (minWS[2] <= maxWS[2]);
            out.MinWS  = out.bValid ? minWS : FVector3f(0.0f);
            out.MaxWS  = out.bValid ? maxWS : FVector3f(0.0f);
            return out;
        }

        struct FDeferredSharedResources {
            RenderCore::FShaderRegistry                       Registry;
            RenderCore::FShaderRegistry::FShaderKey           OutputVSKey;
            RenderCore::FShaderRegistry::FShaderKey           OutputPSKey;
            RenderCore::FShaderRegistry::FShaderKey           LightingVSKey;
            RenderCore::FShaderRegistry::FShaderKey           LightingPSKey;
            RenderCore::FShaderRegistry::FShaderKey           SsaoVSKey;
            RenderCore::FShaderRegistry::FShaderKey           SsaoPSKey;
            RenderCore::FShaderRegistry::FShaderKey           SkyBoxVSKey;
            RenderCore::FShaderRegistry::FShaderKey           SkyBoxPSKey;
            RenderCore::FShaderRegistry::FShaderKey           AtmosphereSkyVSKey;
            RenderCore::FShaderRegistry::FShaderKey           AtmosphereSkyPSKey;
            RenderCore::FMaterialPassDesc                     DefaultPassDesc;
            RenderCore::FMaterialPassDesc                     DefaultShadowPassDesc;
            Container::TShared<RenderCore::FMaterialTemplate> DefaultTemplate;

            Rhi::FRhiBindGroupLayoutRef                       PerFrameLayout;
            Rhi::FRhiBindGroupLayoutRef                       PerDrawLayout;
            u32                                               PerFrameBinding = 0U;
            u32                                               PerDrawBinding  = 0U;
            Rhi::FRhiBindGroupLayoutRef                       OutputLayout;
            Rhi::FRhiSamplerRef                               OutputSampler;
            Rhi::FRhiPipelineLayoutRef                        OutputPipelineLayout;
            Rhi::FRhiPipelineRef                              OutputPipeline;

            // Deferred lighting (FSQ -> BackBuffer).
            Rhi::FRhiBindGroupLayoutRef                       LightingLayout;
            RenderCore::ShaderBinding::FBindingLookupTable    LightingBindings;
            Rhi::FRhiPipelineLayoutRef                        LightingPipelineLayout;
            Rhi::FRhiPipelineRef                              LightingPipeline;

            // SSAO (FSQ -> AO texture).
            Rhi::FRhiBindGroupLayoutRef                       SsaoLayout;
            RenderCore::ShaderBinding::FBindingLookupTable    SsaoBindings;
            Rhi::FRhiPipelineLayoutRef                        SsaoPipelineLayout;
            Rhi::FRhiPipelineRef                              SsaoPipeline;

            // IBL fallbacks (used when view has no sky IBL resources bound).
            Rhi::FRhiTextureRef                               IblBlackCube;
            Rhi::FRhiTextureRef                               IblBlack2D;

            // Persistent CSM shadow map to avoid per-frame large texture churn.
            Rhi::FRhiTextureRef                               ShadowMapCSM;
            u32                                               ShadowMapCSMSize   = 0U;
            u32                                               ShadowMapCSMLayers = 0U;

            // Skybox (FSQ -> SceneColorHDR).
            Rhi::FRhiBindGroupLayoutRef                       SkyBoxLayout;
            RenderCore::ShaderBinding::FBindingLookupTable    SkyBoxBindings;
            Rhi::FRhiPipelineLayoutRef                        SkyBoxPipelineLayout;
            Rhi::FRhiPipelineRef                              SkyBoxPipeline;

            // Atmosphere sky (FSQ -> SceneColorHDR).
            Rhi::FRhiBindGroupLayoutRef                       AtmosphereSkyLayout;
            RenderCore::ShaderBinding::FBindingLookupTable    AtmosphereSkyBindings;
            Rhi::FRhiPipelineLayoutRef                        AtmosphereSkyPipelineLayout;
            Rhi::FRhiPipelineRef                              AtmosphereSkyPipeline;

            THashMap<u64, Rhi::FRhiPipelineRef>               BasePipelines;
            THashMap<u64, Rhi::FRhiPipelineRef>               ShadowPipelines;
            THashMap<u64, Rhi::FRhiBindGroupLayoutRef>        MaterialLayouts;
            THashMap<u64, Rhi::FRhiPipelineLayoutRef>         BasePipelineLayouts;
            Rhi::FRhiVertexLayoutDesc                         BaseVertexLayout;
            bool                                              bBaseVertexLayoutReady = false;
        };

        auto GetSharedResources() -> FDeferredSharedResources& {
            static FDeferredSharedResources resources;
            return resources;
        }

        auto BuildLayoutHash(const TVector<Rhi::FRhiBindGroupLayoutEntry>& entries, u32 setIndex)
            -> u64 {
            constexpr u64 kOffset = 1469598103934665603ULL;
            constexpr u64 kPrime  = 1099511628211ULL;
            u64           hash    = kOffset;
            auto          mix     = [&](u64 value) { hash = (hash ^ value) * kPrime; };

            mix(setIndex);
            for (const auto& entry : entries) {
                mix(entry.mBinding);
                mix(static_cast<u64>(entry.mType));
                mix(static_cast<u64>(entry.mVisibility));
                mix(entry.mArrayCount);
                mix(entry.mHasDynamicOffset ? 1ULL : 0ULL);
            }
            return hash;
        }

        [[nodiscard]] auto IsVulkanBackend() noexcept -> bool {
            return Rhi::RHIGetBackend() == Rhi::ERhiBackend::Vulkan;
        }

        [[nodiscard]] constexpr auto GetPerDrawInstanceCapacity() noexcept -> u32 {
            // Covers base pass + 4 shadow cascades for typical scenes with headroom.
            // TODO: Refactor
            return 16384U;
        }

        [[nodiscard]] auto CountDrawListInstances(
            const RenderCore::Render::FDrawList* drawList) noexcept -> u32 {
            if (drawList == nullptr) {
                return 0U;
            }

            u32 totalInstanceCount = 0U;
            drawList->ForEachBatch([&totalInstanceCount](const auto& batch) {
                totalInstanceCount += static_cast<u32>(batch.mInstances.Size());
            });
            return totalInstanceCount;
        }

        void RebuildPerDrawBindGroups(Rhi::FRhiDevice& device,
            const FDeferredSharedResources& resources, Rhi::FRhiBuffer* perDrawBuffer,
            u32 perDrawStrideBytes, u32 perDrawCapacity,
            TArray<Rhi::FRhiBindGroupRef, 4U>& outGroups) {
            for (auto& group : outGroups) {
                group.Reset();
            }

            if (perDrawBuffer == nullptr || resources.PerDrawLayout.Get() == nullptr) {
                return;
            }

            constexpr u32 kPerDrawFrameRing = 4U;
            const u32     slotCount         = IsVulkanBackend() ? kPerDrawFrameRing : 1U;
            const u64     slotSizeBytes =
                static_cast<u64>(perDrawStrideBytes) * static_cast<u64>(perDrawCapacity);
            for (u32 slotIndex = 0U; slotIndex < slotCount; ++slotIndex) {
                const u64 slotOffsetBytes = slotSizeBytes * static_cast<u64>(slotIndex);

                RenderCore::ShaderBinding::FBindGroupBuilder builder(resources.PerDrawLayout.Get());
                DebugAssert(builder.AddSampledBuffer(resources.PerDrawBinding, perDrawBuffer,
                                slotOffsetBytes, slotSizeBytes),
                    TEXT("BasicDeferredRenderer"),
                    "Failed to add per-draw instance buffer binding (binding={}, slot={}, offset={}, size={}).",
                    resources.PerDrawBinding, slotIndex, slotOffsetBytes, slotSizeBytes);
                Rhi::FRhiBindGroupDesc groupDesc{};
                DebugAssert(builder.Build(groupDesc), TEXT("BasicDeferredRenderer"),
                    "Failed to build per-draw bind group desc from layout (slot={}).", slotIndex);
                outGroups[slotIndex] = device.CreateBindGroup(groupDesc);
            }
        }

        auto BuildMaterialBindGroupLayoutDesc(const RenderCore::FMaterialLayout& materialLayout,
            TVector<Rhi::FRhiBindGroupLayoutEntry>& outEntries) -> u64 {
            outEntries.Clear();

            if (materialLayout.mPropertyBag.IsValid()) {
                Rhi::FRhiBindGroupLayoutEntry entry{};
                entry.mBinding          = materialLayout.mPropertyBag.GetBinding();
                entry.mType             = Rhi::ERhiBindingType::ConstantBuffer;
                entry.mVisibility       = Rhi::ERhiShaderStageFlags::All;
                entry.mArrayCount       = 1U;
                entry.mHasDynamicOffset = false;
                outEntries.PushBack(entry);
            }

            const usize textureCount = materialLayout.mTextureBindings.Size();
            for (usize i = 0U; i < textureCount; ++i) {
                Rhi::FRhiBindGroupLayoutEntry entry{};
                entry.mBinding          = materialLayout.mTextureBindings[i];
                entry.mType             = Rhi::ERhiBindingType::SampledTexture;
                entry.mVisibility       = Rhi::ERhiShaderStageFlags::All;
                entry.mArrayCount       = 1U;
                entry.mHasDynamicOffset = false;
                outEntries.PushBack(entry);
            }

            const usize samplerCount = materialLayout.mSamplerBindings.Size();
            for (usize i = 0U; i < samplerCount; ++i) {
                const u32 samplerBinding = materialLayout.mSamplerBindings[i];
                if (samplerBinding == RenderCore::kMaterialInvalidBinding) {
                    continue;
                }
                Rhi::FRhiBindGroupLayoutEntry entry{};
                entry.mBinding          = samplerBinding;
                entry.mType             = Rhi::ERhiBindingType::Sampler;
                entry.mVisibility       = Rhi::ERhiShaderStageFlags::All;
                entry.mArrayCount       = 1U;
                entry.mHasDynamicOffset = false;
                outEntries.PushBack(entry);
            }

            Core::Algorithm::Sort(
                outEntries.begin(), outEntries.end(), [](const auto& lhs, const auto& rhs) {
                    if (lhs.mBinding != rhs.mBinding) {
                        return lhs.mBinding < rhs.mBinding;
                    }
                    return lhs.mType < rhs.mType;
                });

            // D3D11 backend does not expose descriptor sets/spaces in reflection in our pipeline;
            // we treat all resources as set 0 and separate them by register index ranges (b0/b4/b8,
            // t0/t16/t32, ...). Therefore the "setIndex" must stay 0 here.
            return BuildLayoutHash(outEntries, 0U);
        }

        void UpdateDefaultPassDesc(FDeferredSharedResources& resources) {
            if (!resources.DefaultTemplate) {
                return;
            }
            if (const auto* baseDesc =
                    resources.DefaultTemplate->FindPassDesc(EMaterialPass::BasePass)) {
                resources.DefaultPassDesc = *baseDesc;
            } else if (const auto* anyDesc = resources.DefaultTemplate->FindAnyPassDesc()) {
                resources.DefaultPassDesc = *anyDesc;
            }

            if (const auto* shadowDesc =
                    resources.DefaultTemplate->FindPassDesc(EMaterialPass::ShadowPass)) {
                resources.DefaultShadowPassDesc = *shadowDesc;
            }
        }

        void EnsureDefaultTemplate(FDeferredSharedResources& resources) {
            UpdateDefaultPassDesc(resources);
        }

        auto EnsureFullscreenPipelineFromKeys(Rhi::FRhiDevice& device,
            FDeferredSharedResources&                          resources,
            const RenderCore::FShaderRegistry::FShaderKey&     vsKey,
            const RenderCore::FShaderRegistry::FShaderKey&     psKey,
            Rhi::FRhiPipelineLayoutRef& pipelineLayout, Rhi::FRhiPipelineRef& outPipeline,
            const TChar* passLabel, const TChar* pipelineDebugName,
            bool bOptionalWhenShaderNotConfigured) -> bool {
            if (outPipeline) {
                return true;
            }

            if (!vsKey.IsValid() || !psKey.IsValid()) {
                if (!bOptionalWhenShaderNotConfigured) {
                    LogErrorCat(TEXT("Rendering.BasicDeferred"),
                        TEXT("{} shaders are not configured."), passLabel);
                }
                return false;
            }

            auto vs = resources.Registry.FindShader(vsKey);
            auto ps = resources.Registry.FindShader(psKey);
            if (!vs || !ps) {
                LogErrorCat(TEXT("Rendering.BasicDeferred"), TEXT("{} shaders are not registered."),
                    passLabel);
                return false;
            }

            if (!pipelineLayout) {
                LogErrorCat(TEXT("Rendering.BasicDeferred"), TEXT("{} pipeline layout is missing."),
                    passLabel);
                return false;
            }

            FRendererGraphicsPipelineBuildInputs buildInputs{};
            buildInputs.mPreset         = ERendererGraphicsPipelinePreset::Fullscreen;
            buildInputs.mDebugName      = pipelineDebugName;
            buildInputs.mPipelineLayout = pipelineLayout.Get();
            buildInputs.mVertexShader   = vs.Get();
            buildInputs.mPixelShader    = ps.Get();

            Rhi::FRhiGraphicsPipelineDesc desc{};
            if (!BuildGraphicsPipelineDesc(buildInputs, desc)) {
                return false;
            }

            outPipeline = device.CreateGraphicsPipeline(desc);
            return outPipeline.Get() != nullptr;
        }

        auto BuildShaderKeys(const RenderCore::FShaderRegistry::FShaderKey& vsKey,
            const RenderCore::FShaderRegistry::FShaderKey&                  psKey)
            -> TVector<RenderCore::FShaderRegistry::FShaderKey> {
            TVector<RenderCore::FShaderRegistry::FShaderKey> shaderKeys{};
            if (vsKey.IsValid()) {
                shaderKeys.PushBack(vsKey);
            }
            if (psKey.IsValid()) {
                shaderKeys.PushBack(psKey);
            }
            return shaderKeys;
        }

        auto EnsureReflectedBindGroupLayout(Rhi::FRhiDevice&        device,
            FDeferredSharedResources&                               resources,
            const TVector<RenderCore::FShaderRegistry::FShaderKey>& shaderKeys, u32 setIndex,
            Rhi::FRhiBindGroupLayoutRef&                    outLayout,
            RenderCore::ShaderBinding::FBindingLookupTable* outLookup,
            const TChar* layoutErrorMessage, const TChar* lookupErrorMessage,
            bool bAllowEmptyShaderKeys = false) -> bool {
            if (outLayout) {
                return true;
            }
            if (shaderKeys.IsEmpty()) {
                if (!bAllowEmptyShaderKeys) {
                    DebugAssert(false, TEXT("BasicDeferredRenderer"), "{}", layoutErrorMessage);
                }
                return false;
            }

            Rhi::FRhiBindGroupLayoutDesc layoutDesc{};
            const bool built = RenderCore::ShaderBinding::BuildBindGroupLayoutFromShaderSet(
                resources.Registry, shaderKeys, setIndex, layoutDesc);
            DebugAssert(built, TEXT("BasicDeferredRenderer"), "{}", layoutErrorMessage);
            if (!built) {
                return false;
            }

            outLayout = device.CreateBindGroupLayout(layoutDesc);
            if (!outLayout) {
                return false;
            }

            if (outLookup != nullptr) {
                const bool builtLookup = RenderCore::ShaderBinding::BuildBindingLookupTable(
                    resources.Registry, shaderKeys, setIndex, outLayout.Get(), *outLookup);
                DebugAssert(builtLookup, TEXT("BasicDeferredRenderer"), "{}", lookupErrorMessage);
                return builtLookup;
            }
            return true;
        }

        auto EnsureSingleReflectedBindingLayout(Rhi::FRhiDevice&    device,
            FDeferredSharedResources&                               resources,
            const TVector<RenderCore::FShaderRegistry::FShaderKey>& shaderKeys,
            FStringView bindingName, Rhi::ERhiBindingType bindingType, bool bConstantBuffer,
            Rhi::FRhiBindGroupLayoutRef& outLayout, u32& outBinding, const TChar* errorMessage,
            FStringView fallbackBindingName = {}) -> bool {
            if (outLayout) {
                return true;
            }
            if (shaderKeys.IsEmpty()) {
                DebugAssert(false, TEXT("BasicDeferredRenderer"), "{}", errorMessage);
                return false;
            }

            u32                       setIndex   = 0U;
            u32                       binding    = 0U;
            Rhi::ERhiShaderStageFlags visibility = Rhi::ERhiShaderStageFlags::All;
            bool                      found      = bConstantBuffer
                                          ? RenderCore::ShaderBinding::ResolveConstantBufferBindingByName(
                      resources.Registry, shaderKeys, bindingName, setIndex, binding, visibility)
                                          : RenderCore::ShaderBinding::ResolveResourceBindingByName(resources.Registry,
                                                shaderKeys, bindingName, bindingType, setIndex, binding, visibility);
            if (!found && !fallbackBindingName.IsEmpty()) {
                found = bConstantBuffer
                    ? RenderCore::ShaderBinding::ResolveConstantBufferBindingByName(
                          resources.Registry, shaderKeys, fallbackBindingName, setIndex, binding,
                          visibility)
                    : RenderCore::ShaderBinding::ResolveResourceBindingByName(resources.Registry,
                          shaderKeys, fallbackBindingName, bindingType, setIndex, binding,
                          visibility);
            }

            DebugAssert(found, TEXT("BasicDeferredRenderer"), "{}", errorMessage);
            if (!found) {
                return false;
            }

            Rhi::FRhiBindGroupLayoutDesc layoutDesc{};
            layoutDesc.mSetIndex = setIndex;
            Rhi::FRhiBindGroupLayoutEntry entry{};
            entry.mBinding    = binding;
            entry.mType       = bindingType;
            entry.mVisibility = visibility;
            layoutDesc.mEntries.PushBack(entry);
            layoutDesc.mLayoutHash = BuildLayoutHash(layoutDesc.mEntries, layoutDesc.mSetIndex);
            outLayout              = device.CreateBindGroupLayout(layoutDesc);
            if (!outLayout) {
                return false;
            }

            outBinding = binding;
            return true;
        }

        void EnsurePipelineLayoutFromSingleBindGroup(Rhi::FRhiDevice& device,
            Rhi::FRhiBindGroupLayoutRef&                              bindGroupLayout,
            Rhi::FRhiPipelineLayoutRef&                               outPipelineLayout) {
            if (outPipelineLayout) {
                return;
            }
            Rhi::FRhiPipelineLayoutDesc layoutDesc{};
            if (bindGroupLayout) {
                layoutDesc.mBindGroupLayouts.PushBack(bindGroupLayout.Get());
            }
            outPipelineLayout = device.CreatePipelineLayout(layoutDesc);
        }

        void EnsureLayouts(Rhi::FRhiDevice& device, FDeferredSharedResources& resources) {
            TVector<RenderCore::FShaderRegistry::FShaderKey> basePassShaderKeys;
            if (resources.DefaultPassDesc.mShaders.mVertex.IsValid()) {
                basePassShaderKeys.PushBack(resources.DefaultPassDesc.mShaders.mVertex);
            }
            if (resources.DefaultPassDesc.mShaders.mPixel.IsValid()) {
                basePassShaderKeys.PushBack(resources.DefaultPassDesc.mShaders.mPixel);
            }
            if (resources.DefaultShadowPassDesc.mShaders.mVertex.IsValid()) {
                basePassShaderKeys.PushBack(resources.DefaultShadowPassDesc.mShaders.mVertex);
            }
            if (resources.DefaultShadowPassDesc.mShaders.mPixel.IsValid()) {
                basePassShaderKeys.PushBack(resources.DefaultShadowPassDesc.mShaders.mPixel);
            }

            if (!resources.PerFrameLayout) {
                (void)EnsureSingleReflectedBindingLayout(device, resources, basePassShaderKeys,
                    TEXT("ViewConstants"), Rhi::ERhiBindingType::ConstantBuffer, true,
                    resources.PerFrameLayout, resources.PerFrameBinding,
                    TEXT("Failed to resolve per-frame cbuffer binding from shader reflection."),
                    TEXT("DeferredView"));
            }

            if (!resources.PerDrawLayout) {
                (void)EnsureSingleReflectedBindingLayout(device, resources, basePassShaderKeys,
                    TEXT("InstanceDataBuffer"), Rhi::ERhiBindingType::SampledBuffer, false,
                    resources.PerDrawLayout, resources.PerDrawBinding,
                    TEXT(
                        "Failed to resolve per-draw instance buffer binding from shader reflection."));
            }

            if (!resources.OutputLayout) {
                const auto shaderKeys =
                    BuildShaderKeys(resources.OutputVSKey, resources.OutputPSKey);
                (void)EnsureReflectedBindGroupLayout(device, resources, shaderKeys, 0U,
                    resources.OutputLayout, nullptr,
                    TEXT("Failed to build output bind group layout from shader reflection."),
                    TEXT(""), true);
            }

            if (!resources.OutputSampler) {
                Rhi::FRhiSamplerDesc samplerDesc{};
                resources.OutputSampler = Rhi::RHICreateSampler(samplerDesc);
            }

            if (!resources.OutputPipelineLayout) {
                EnsurePipelineLayoutFromSingleBindGroup(
                    device, resources.OutputLayout, resources.OutputPipelineLayout);
            }

            if (!resources.LightingLayout) {
                const auto shaderKeys =
                    BuildShaderKeys(resources.LightingVSKey, resources.LightingPSKey);
                (void)EnsureReflectedBindGroupLayout(device, resources, shaderKeys, 0U,
                    resources.LightingLayout, &resources.LightingBindings,
                    TEXT("Failed to build lighting bind group layout from shader reflection."),
                    TEXT("Failed to build lighting binding lookup table from shader reflection."));
            }

            if (!resources.SsaoLayout) {
                const auto shaderKeys = BuildShaderKeys(resources.SsaoVSKey, resources.SsaoPSKey);
                if (!shaderKeys.IsEmpty()) {
                    u32                       ssaoSetIndex        = 0U;
                    u32                       deferredViewBinding = 0U;
                    Rhi::ERhiShaderStageFlags deferredViewVisibility =
                        Rhi::ERhiShaderStageFlags::None;
                    const bool foundDeferredView =
                        RenderCore::ShaderBinding::ResolveConstantBufferBindingByName(
                            resources.Registry, shaderKeys, TEXT("DeferredView"), ssaoSetIndex,
                            deferredViewBinding, deferredViewVisibility);
                    (void)deferredViewBinding;
                    (void)deferredViewVisibility;
                    DebugAssert(foundDeferredView, TEXT("BasicDeferredRenderer"),
                        "Failed to resolve SSAO binding set from DeferredView.");
                    if (foundDeferredView) {
                        (void)EnsureReflectedBindGroupLayout(device, resources, shaderKeys,
                            ssaoSetIndex, resources.SsaoLayout, &resources.SsaoBindings,
                            TEXT("Failed to build SSAO bind group layout from shader reflection."),
                            TEXT(
                                "Failed to build SSAO binding lookup table from shader reflection."));
                    }
                }
            }

            if (!resources.IblBlackCube || !resources.IblBlack2D) {
                auto* rhiDevice = Rhi::RHIGetDevice();
                if (rhiDevice != nullptr) {
                    if (!resources.IblBlackCube) {
                        Rhi::FRhiTextureDesc texDesc{};
                        texDesc.mDebugName.Assign(TEXT("IBL.BlackCube"));
                        texDesc.mDimension     = Rhi::ERhiTextureDimension::Cube;
                        texDesc.mWidth         = 1U;
                        texDesc.mHeight        = 1U;
                        texDesc.mMipLevels     = 1U;
                        texDesc.mArrayLayers   = 6U;
                        texDesc.mFormat        = Rhi::ERhiFormat::R16G16B16A16Float;
                        texDesc.mBindFlags     = Rhi::ERhiTextureBindFlags::ShaderResource;
                        resources.IblBlackCube = Rhi::RHICreateTexture(texDesc);
                        if (resources.IblBlackCube) {
                            const u16 pixel[4] = { 0, 0, 0, 0 };
                            for (u32 face = 0U; face < 6U; ++face) {
                                Rhi::FRhiTextureSubresource sub{};
                                sub.mMipLevel   = 0U;
                                sub.mArrayLayer = face;
                                rhiDevice->UpdateTextureSubresource(
                                    resources.IblBlackCube.Get(), sub, pixel, 8U, 8U);
                            }
                        }
                    }
                    if (!resources.IblBlack2D) {
                        Rhi::FRhiTextureDesc texDesc{};
                        texDesc.mDebugName.Assign(TEXT("IBL.Black2D"));
                        texDesc.mDimension   = Rhi::ERhiTextureDimension::Tex2D;
                        texDesc.mWidth       = 1U;
                        texDesc.mHeight      = 1U;
                        texDesc.mMipLevels   = 1U;
                        texDesc.mFormat      = Rhi::ERhiFormat::R8G8B8A8Unorm;
                        texDesc.mBindFlags   = Rhi::ERhiTextureBindFlags::ShaderResource;
                        resources.IblBlack2D = Rhi::RHICreateTexture(texDesc);
                        if (resources.IblBlack2D) {
                            const u8 pixel[4] = { 0, 0, 0, 0 };
                            rhiDevice->UpdateTextureSubresource(resources.IblBlack2D.Get(),
                                Rhi::FRhiTextureSubresource{}, pixel, 4U, 4U);
                        }
                    }
                }
            }

            if (!resources.LightingPipelineLayout) {
                EnsurePipelineLayoutFromSingleBindGroup(
                    device, resources.LightingLayout, resources.LightingPipelineLayout);
            }

            if (!resources.SsaoPipelineLayout) {
                EnsurePipelineLayoutFromSingleBindGroup(
                    device, resources.SsaoLayout, resources.SsaoPipelineLayout);
            }

            if (!resources.SkyBoxLayout) {
                const auto shaderKeys =
                    BuildShaderKeys(resources.SkyBoxVSKey, resources.SkyBoxPSKey);
                (void)EnsureReflectedBindGroupLayout(device, resources, shaderKeys, 0U,
                    resources.SkyBoxLayout, &resources.SkyBoxBindings,
                    TEXT("Failed to build skybox bind group layout from shader reflection."),
                    TEXT("Failed to build skybox binding lookup table from shader reflection."));
            }

            if (!resources.SkyBoxPipelineLayout) {
                EnsurePipelineLayoutFromSingleBindGroup(
                    device, resources.SkyBoxLayout, resources.SkyBoxPipelineLayout);
            }

            if (!resources.AtmosphereSkyLayout) {
                const auto shaderKeys =
                    BuildShaderKeys(resources.AtmosphereSkyVSKey, resources.AtmosphereSkyPSKey);
                (void)EnsureReflectedBindGroupLayout(device, resources, shaderKeys, 0U,
                    resources.AtmosphereSkyLayout, &resources.AtmosphereSkyBindings,
                    TEXT(
                        "Failed to build atmosphere sky bind group layout from shader reflection."),
                    TEXT(
                        "Failed to build atmosphere sky binding lookup table from shader reflection."));
            }

            if (!resources.AtmosphereSkyPipelineLayout) {
                EnsurePipelineLayoutFromSingleBindGroup(
                    device, resources.AtmosphereSkyLayout, resources.AtmosphereSkyPipelineLayout);
            }
        }

        void EnsureVertexLayout(FDeferredSharedResources& resources) {
            if (resources.bBaseVertexLayoutReady) {
                return;
            }

            const i32 useReflection = rVertexLayoutUseShaderReflection.GetRenderValue();
            if (useReflection != 0) {
                constexpr const TChar* kFactoryName = TEXT("StaticMeshVertexFactory");
                TVector<RenderCore::FShaderRegistry::FShaderKey> shaderKeys{};
                if (resources.DefaultPassDesc.mShaders.mVertex.IsValid()) {
                    shaderKeys.PushBack(resources.DefaultPassDesc.mShaders.mVertex);
                }
                if (resources.DefaultPassDesc.mShaders.mPixel.IsValid()) {
                    shaderKeys.PushBack(resources.DefaultPassDesc.mShaders.mPixel);
                }

                RenderCore::Geometry::FShaderVertexInputRequirement requirement{};
                FString                                             requirementError{};
                const bool                                          requirementBuilt =
                    RenderCore::Geometry::BuildShaderVertexInputRequirementFromShaderSet(
                        resources.Registry, shaderKeys, requirement, &requirementError);

                RenderCore::Geometry::FVertexFactoryProvidedLayout provided{};
                const bool                                         providedBuilt =
                    RenderCore::Geometry::BuildStaticMeshProvidedLayout(provided);
                DebugAssert(providedBuilt, TEXT("BasicDeferredRenderer"),
                    "Failed to build StaticMesh provided vertex layout.");

                RenderCore::Geometry::FResolvedVertexLayout resolved{};
                FString                                     resolveError{};
                const bool resolvedOk = requirementBuilt && providedBuilt
                    && RenderCore::Geometry::ValidateAndBuildVertexLayout(
                        requirement, provided, resolved, &resolveError);
                if (resolvedOk) {
                    resources.BaseVertexLayout       = Move(resolved.mVertexLayout);
                    resources.bBaseVertexLayoutReady = true;
                    return;
                }

                FString shaderLabel{};
                for (usize i = 0U; i < shaderKeys.Size(); ++i) {
                    if (i > 0U) {
                        shaderLabel.Append(TEXT(","));
                    }
                    shaderLabel.Append(shaderKeys[i].mName.ToView());
                }
                if (shaderLabel.IsEmptyString()) {
                    shaderLabel.Assign(TEXT("<none>"));
                }

                LogErrorCat(TEXT("BasicDeferredRenderer"),
                    "VertexLayout resolve failed: shader='{}' factory='{}' requirementBuilt={} "
                    "resolved={} requirementError='{}' resolveError='{}'.",
                    shaderLabel.ToView(), kFactoryName, requirementBuilt ? 1U : 0U,
                    resolvedOk ? 1U : 0U, requirementError.ToView(), resolveError.ToView());
                for (const auto& req : requirement.mElements) {
                    const auto semanticName = req.mSemanticName.IsEmptyString()
                        ? TEXT("<unknown>")
                        : req.mSemanticName.CStr();
                    LogErrorCat(TEXT("BasicDeferredRenderer"),
                        "VertexLayout requirement: shader='{}' semantic='{}{}' valueType={} factory='{}'.",
                        shaderLabel, semanticName, req.mSemantic.mSemanticIndex,
                        static_cast<u32>(req.mValueType), kFactoryName);
                }
                for (const auto& prov : provided.mElements) {
                    const auto semanticName = prov.mSemanticName.IsEmptyString()
                        ? TEXT("<unknown>")
                        : prov.mSemanticName.CStr();
                    LogErrorCat(TEXT("BasicDeferredRenderer"),
                        "VertexLayout factory-provided: shader='{}' semantic='{}{}' format={} factory='{}'.",
                        shaderLabel, semanticName, prov.mSemantic.mSemanticIndex,
                        static_cast<u32>(prov.mFormat), kFactoryName);
                }
                LogWarningCat(TEXT("BasicDeferredRenderer"),
                    "Vertex reflection path failed; fallback to legacy vertex layout (shader='{}' factory='{}').",
                    shaderLabel, kFactoryName);
            }

            const bool built =
                RenderCore::Geometry::BuildStaticMeshLegacyVertexLayout(resources.BaseVertexLayout);
            DebugAssert(built, TEXT("BasicDeferredRenderer"),
                "Failed to build base vertex layout from StaticMeshVertexFactory.");
            if (!built) {
                return;
            }
            resources.bBaseVertexLayoutReady = true;
        }

        void UpdateConstantBuffer(Rhi::FRhiBuffer* buffer, const void* data, u64 sizeBytes) {
            if (buffer == nullptr || data == nullptr || sizeBytes == 0ULL) {
                return;
            }
            // TODO: Refactor, consider sync problems
            auto lock = buffer->Lock(0ULL, sizeBytes, Rhi::ERhiBufferLockMode::WriteDiscard);
            if (!lock.IsValid()) {
                return;
            }
            Core::Platform::Generic::Memcpy(lock.mData, data, static_cast<usize>(sizeBytes));
            buffer->Unlock(lock);
        }
        struct FBasePassPipelineData {
            Rhi::FRhiDevice*                     Device          = nullptr;
            RenderCore::FShaderRegistry*         Registry        = nullptr;
            THashMap<u64, Rhi::FRhiPipelineRef>* PipelineCache   = nullptr;
            const RenderCore::FMaterialPassDesc* DefaultPassDesc = nullptr;
            Rhi::FRhiVertexLayoutDesc            VertexLayout;
        };

        auto ResolveBasePassPipeline(const RenderCore::Render::FDrawBatch& batch,
            const RenderCore::FMaterialPassDesc* passDesc, void* userData) -> Rhi::FRhiPipeline* {
            auto* data = static_cast<FBasePassPipelineData*>(userData);
            if (!data || !data->Device || !data->Registry || !data->PipelineCache) {
                return nullptr;
            }

            const auto* resolvedPass = passDesc ? passDesc : data->DefaultPassDesc;
            if (!resolvedPass) {
                return nullptr;
            }

            auto&                                  resources  = GetSharedResources();
            const auto&                            passLayout = resolvedPass->mLayout;

            TVector<Rhi::FRhiBindGroupLayoutEntry> layoutEntries;
            const u64                              materialLayoutHash =
                BuildMaterialBindGroupLayoutDesc(passLayout, layoutEntries);

            Rhi::FRhiBindGroupLayoutRef materialLayoutRef;
            if (const auto it = resources.MaterialLayouts.FindIt(materialLayoutHash);
                it != resources.MaterialLayouts.end()) {
                materialLayoutRef = it->second;
            } else {
                ++gBasePassPipelineStats.mMaterialLayoutMisses;
                Rhi::FRhiBindGroupLayoutDesc layoutDesc{};
                layoutDesc.mSetIndex   = IsVulkanBackend() ? 2U : 0U;
                layoutDesc.mEntries    = layoutEntries;
                layoutDesc.mLayoutHash = BuildLayoutHash(layoutDesc.mEntries, layoutDesc.mSetIndex);
                materialLayoutRef      = data->Device->CreateBindGroupLayout(layoutDesc);
                if (!materialLayoutRef) {
                    return nullptr;
                }
                resources.MaterialLayouts[materialLayoutHash] = materialLayoutRef;
                LogInfoCat(TEXT("Rendering.BasicDeferred"),
                    TEXT("BasePass MaterialLayout entries={} hash={}"),
                    static_cast<u32>(layoutEntries.Size()), materialLayoutHash);
                for (const auto& entry : layoutEntries) {
                    LogInfoCat(TEXT("Rendering.BasicDeferred"),
                        TEXT("  MaterialLayout binding={} type={} vis={}"), entry.mBinding,
                        static_cast<u32>(entry.mType), static_cast<u32>(entry.mVisibility));
                }
            }

            Rhi::FRhiPipelineLayoutRef pipelineLayout;
            if (const auto it = resources.BasePipelineLayouts.FindIt(materialLayoutHash);
                it != resources.BasePipelineLayouts.end()) {
                pipelineLayout = it->second;
            } else {
                ++gBasePassPipelineStats.mPipelineLayoutMisses;
                Rhi::FRhiPipelineLayoutDesc layoutDesc{};
                if (resources.PerFrameLayout) {
                    layoutDesc.mBindGroupLayouts.PushBack(resources.PerFrameLayout.Get());
                }
                if (resources.PerDrawLayout) {
                    layoutDesc.mBindGroupLayouts.PushBack(resources.PerDrawLayout.Get());
                }
                if (materialLayoutRef) {
                    layoutDesc.mBindGroupLayouts.PushBack(materialLayoutRef.Get());
                }
                pipelineLayout = data->Device->CreatePipelineLayout(layoutDesc);
                if (!pipelineLayout) {
                    return nullptr;
                }
                resources.BasePipelineLayouts[materialLayoutHash] = pipelineLayout;
                LogInfoCat(TEXT("Rendering.BasicDeferred"),
                    TEXT("BasePass PipelineLayout groups={} hash={}"),
                    static_cast<u32>(layoutDesc.mBindGroupLayouts.Size()), materialLayoutHash);
            }

            const u64 key =
                batch.mBatchKey.mPipelineKey ^ (materialLayoutHash * 0x9e3779b97f4a7c15ULL);
            if (const auto it = data->PipelineCache->FindIt(key);
                it != data->PipelineCache->end()) {
                if (data->PipelineCache == &resources.ShadowPipelines) {
                    ++gBasePassPipelineStats.mShadowHits;
                } else {
                    ++gBasePassPipelineStats.mBaseHits;
                }
                return it->second.Get();
            }

            const auto         vs = data->Registry->FindShader(resolvedPass->mShaders.mVertex);
            Rhi::FRhiShaderRef ps{};
            if (resolvedPass->mShaders.mPixel.IsValid()) {
                ps = data->Registry->FindShader(resolvedPass->mShaders.mPixel);
            }
            if (!vs) {
                return nullptr;
            }

            FRendererGraphicsPipelineBuildInputs buildInputs{};
            buildInputs.mPreset            = (data->PipelineCache == &resources.ShadowPipelines)
                           ? ERendererGraphicsPipelinePreset::ShadowDepth
                           : ERendererGraphicsPipelinePreset::MeshMaterial;
            buildInputs.mDebugName         = TEXT("BasicDeferred.BasePassPipeline");
            buildInputs.mPipelineLayout    = pipelineLayout.Get();
            buildInputs.mVertexShader      = vs.Get();
            buildInputs.mPixelShader       = ps ? ps.Get() : nullptr;
            buildInputs.mVertexLayout      = &data->VertexLayout;
            buildInputs.mMaterialPassState = &resolvedPass->mState;

            Rhi::FRhiGraphicsPipelineDesc desc{};
            if (!BuildGraphicsPipelineDesc(buildInputs, desc)) {
                return nullptr;
            }

            auto pipeline = data->Device->CreateGraphicsPipeline(desc);
            if (!pipeline) {
                return nullptr;
            }

            if (data->PipelineCache == &resources.ShadowPipelines) {
                ++gBasePassPipelineStats.mShadowMisses;
            } else {
                ++gBasePassPipelineStats.mBaseMisses;
            }
            (*data->PipelineCache)[key] = pipeline;
            return pipeline.Get();
        }

        auto ResolveShadowPassPipeline(const RenderCore::Render::FDrawBatch& batch,
            const RenderCore::FMaterialPassDesc* /*passDesc*/, void* userData)
            -> Rhi::FRhiPipeline* {
            // Always use the renderer's default shadow pass desc, even if a material provides a
            // ShadowPass variant (prevents accidentally binding BasePass MRT shaders while
            // rendering depth-only).
            auto* data = static_cast<FBasePassPipelineData*>(userData);
            if (!data || data->DefaultPassDesc == nullptr) {
                return nullptr;
            }

            // Create a synthetic batch key by forcing the pipeline key to a stable constant so it
            // can't collide with BasePass pipelines.
            RenderCore::Render::FDrawBatch synthetic = batch;
            synthetic.mBatchKey.mPipelineKey         = 0x43534D534841444FULL; // "CSMSHADO"
            auto shadowDesc                          = *data->DefaultPassDesc;
            shadowDesc.mShaders.mPixel               = {};
            auto&                 resources          = GetSharedResources();
            FBasePassPipelineData shadowData         = *data;
            shadowData.PipelineCache                 = &resources.ShadowPipelines;
            return ResolveBasePassPipeline(synthetic, &shadowDesc, &shadowData);
        }

        struct FBasePassBindingData {
            Rhi::FRhiBuffer* PerDrawBuffer          = nullptr;
            u32              PerDrawStrideBytes     = 0U;
            u32              PerDrawCapacity        = 0U;
            u64              PerDrawBaseOffsetBytes = 0ULL;
            u32              PerDrawCursorInstances = 0U;
            bool             bUseInstanceCursor     = false;
        };

        void BindPerDraw(Rhi::FRhiCmdContext& ctx, const RenderCore::Render::FDrawBatch& batch,
            FDrawBatchExecutionParams& outParams, void* userData) {
            auto* data = static_cast<FBasePassBindingData*>(userData);
            if (!data || data->PerDrawBuffer == nullptr) {
                return;
            }

            if (batch.mInstances.IsEmpty()) {
                return;
            }

            DebugAssert(data->PerDrawStrideBytes == static_cast<u32>(sizeof(FInstanceDrawData)),
                TEXT("BasicDeferredRenderer"),
                "PerDraw instance stride invalid (stride={}, expected={}).",
                data->PerDrawStrideBytes, static_cast<u32>(sizeof(FInstanceDrawData)));

            const u32 instanceCount = static_cast<u32>(batch.mInstances.Size());
            u32       firstInstance = 0U;
            if (data->bUseInstanceCursor) {
                firstInstance = data->PerDrawCursorInstances;
                DebugAssert(firstInstance <= data->PerDrawCapacity, TEXT("BasicDeferredRenderer"),
                    "PerDraw first-instance out of range (firstInstance={}, capacity={}).",
                    firstInstance, data->PerDrawCapacity);
                const u32 remainingCapacity = (firstInstance <= data->PerDrawCapacity)
                    ? (data->PerDrawCapacity - firstInstance)
                    : 0U;
                DebugAssert(instanceCount <= remainingCapacity, TEXT("BasicDeferredRenderer"),
                    "PerDraw instance buffer overflow (firstInstance={}, instanceCount={}, capacity={}).",
                    firstInstance, instanceCount, data->PerDrawCapacity);
            } else {
                DebugAssert(instanceCount <= data->PerDrawCapacity, TEXT("BasicDeferredRenderer"),
                    "PerDraw instance buffer overflow (instanceCount={}, capacity={}).",
                    instanceCount, data->PerDrawCapacity);
            }

            static thread_local TVector<FInstanceDrawData> uploadData{};
            uploadData.Clear();
            uploadData.Reserve(instanceCount);
            for (const auto& instance : batch.mInstances) {
                FInstanceDrawData gpuData{};
                gpuData.World        = instance.mWorld;
                gpuData.NormalMatrix = Core::Math::LinAlg::ComputeNormalMatrix(instance.mWorld);
                uploadData.PushBack(Move(gpuData));
            }

            const u64 byteOffset = data->bUseInstanceCursor
                ? (data->PerDrawBaseOffsetBytes
                      + static_cast<u64>(firstInstance)
                          * static_cast<u64>(sizeof(FInstanceDrawData)))
                : data->PerDrawBaseOffsetBytes;
            const u64 byteSize =
                static_cast<u64>(instanceCount) * static_cast<u64>(sizeof(FInstanceDrawData));
            ctx.RHIUpdateDynamicBufferDiscard(
                data->PerDrawBuffer, uploadData.Data(), byteSize, byteOffset);
            outParams.mFirstInstance = data->bUseInstanceCursor ? firstInstance : 0U;
            if (data->bUseInstanceCursor) {
                data->PerDrawCursorInstances += instanceCount;
            }
        }

        [[nodiscard]] auto BuildDeferredLightingPassInputs(RenderCore::FFrameGraph& graph,
            Rhi::FRhiDevice* device, FDeferredSharedResources& resources,
            const RenderCore::View::FViewRect& viewRect,
            const FPerFrameConstants& perFrameConstants, const FRenderViewContext& viewContext,
            Rhi::FRhiBuffer* perFrameBuffer, Rhi::FRhiBuffer* iblConstantsBuffer,
            RenderCore::FFrameGraphTextureRef gbufferA, RenderCore::FFrameGraphTextureRef gbufferB,
            RenderCore::FFrameGraphTextureRef gbufferC,
            RenderCore::FFrameGraphTextureRef sceneDepth,
            RenderCore::FFrameGraphTextureRef ssaoTexture,
            RenderCore::FFrameGraphTextureRef shadowMap, u32 width, u32 height,
            Deferred::FDeferredLightingPassInputs& outInputs) -> bool {
            const bool bHasLightingPipeline = (device != nullptr)
                && EnsureFullscreenPipelineFromKeys(*device, resources, resources.LightingVSKey,
                    resources.LightingPSKey, resources.LightingPipelineLayout,
                    resources.LightingPipeline, TEXT("Deferred lighting"),
                    TEXT("BasicDeferred.DeferredLightingPipeline"), false);
            if (device == nullptr || !bHasLightingPipeline || !resources.LightingLayout
                || !resources.LightingPipeline || !resources.OutputSampler) {
                return false;
            }

            outInputs.Graph              = &graph;
            outInputs.ViewRect           = &viewRect;
            outInputs.PerFrameConstants  = &perFrameConstants;
            outInputs.Pipeline           = resources.LightingPipeline.Get();
            outInputs.Layout             = resources.LightingLayout.Get();
            outInputs.Sampler            = resources.OutputSampler.Get();
            outInputs.Bindings           = &resources.LightingBindings;
            outInputs.PerFrameBuffer     = perFrameBuffer;
            outInputs.IblConstantsBuffer = iblConstantsBuffer;
            outInputs.GBufferA           = gbufferA;
            outInputs.GBufferB           = gbufferB;
            outInputs.GBufferC           = gbufferC;
            outInputs.SceneDepth         = sceneDepth;
            outInputs.SsaoTexture        = ssaoTexture;
            outInputs.ShadowMap          = shadowMap;
            outInputs.Width              = width;
            outInputs.Height             = height;
            outInputs.SkyIrradiance      = viewContext.SkyIrradianceCube;
            outInputs.SkySpecular        = viewContext.SkySpecularCube;
            outInputs.BrdfLut            = viewContext.BrdfLutTexture;
            outInputs.IblBlackCube       = resources.IblBlackCube;
            outInputs.IblBlack2D         = resources.IblBlack2D;

            // Keep IBL disabled by default; runtime tuning can be enabled later.
            outInputs.RuntimeSettings.bEnableIbl           = false;
            outInputs.RuntimeSettings.IblDiffuseIntensity  = rIblDiffuseIntensity.GetRenderValue();
            outInputs.RuntimeSettings.IblSpecularIntensity = rIblSpecularIntensity.GetRenderValue();
            outInputs.RuntimeSettings.IblSaturation        = rIblSaturation.GetRenderValue();
            outInputs.RuntimeSettings.SpecularMaxLod       = viewContext.SkySpecularMaxLod;
            return true;
        }

        [[nodiscard]] auto BuildDeferredAtmosphereSkyPassInputs(RenderCore::FFrameGraph& graph,
            Rhi::FRhiDevice* device, FDeferredSharedResources& resources,
            const RenderCore::View::FViewRect& viewRect, const FRenderViewContext& viewContext,
            Rhi::FRhiBuffer* perFrameBuffer, RenderCore::FFrameGraphTextureRef sceneDepth,
            RenderCore::FFrameGraphTextureRef           sceneColorHDR,
            Deferred::FDeferredAtmosphereSkyPassInputs& outInputs) -> bool {
            const bool bHasAtmosphereSkyPipeline = (device != nullptr)
                && EnsureFullscreenPipelineFromKeys(*device, resources,
                    resources.AtmosphereSkyVSKey, resources.AtmosphereSkyPSKey,
                    resources.AtmosphereSkyPipelineLayout, resources.AtmosphereSkyPipeline,
                    TEXT("Deferred atmosphere sky"), TEXT("BasicDeferred.AtmosphereSkyPipeline"),
                    true);
            if (!viewContext.bHasAtmosphereSky || (viewContext.AtmosphereParamsBuffer == nullptr)
                || !viewContext.AtmosphereTransmittanceLut || !viewContext.AtmosphereScatteringLut
                || !viewContext.AtmosphereSingleMieScatteringLut || !sceneColorHDR.IsValid()
                || !sceneDepth.IsValid() || device == nullptr || !resources.AtmosphereSkyLayout
                || !bHasAtmosphereSkyPipeline || !resources.AtmosphereSkyPipeline
                || !resources.OutputSampler) {
                return false;
            }

            outInputs.Graph                  = &graph;
            outInputs.ViewRect               = &viewRect;
            outInputs.Pipeline               = resources.AtmosphereSkyPipeline.Get();
            outInputs.Layout                 = resources.AtmosphereSkyLayout.Get();
            outInputs.Sampler                = resources.OutputSampler.Get();
            outInputs.Bindings               = &resources.AtmosphereSkyBindings;
            outInputs.PerFrameBuffer         = perFrameBuffer;
            outInputs.AtmosphereParamsBuffer = viewContext.AtmosphereParamsBuffer;
            outInputs.TransmittanceLut       = viewContext.AtmosphereTransmittanceLut;
            outInputs.ScatteringLut          = viewContext.AtmosphereScatteringLut;
            outInputs.SingleMieScatteringLut = viewContext.AtmosphereSingleMieScatteringLut;
            outInputs.SceneDepth             = sceneDepth;
            outInputs.SceneColorHDR          = sceneColorHDR;
            return true;
        }

        [[nodiscard]] auto BuildDeferredSkyBoxPassInputs(RenderCore::FFrameGraph& graph,
            Rhi::FRhiDevice* device, FDeferredSharedResources& resources,
            const RenderCore::View::FViewRect& viewRect, const FRenderViewContext& viewContext,
            Rhi::FRhiBuffer* perFrameBuffer, RenderCore::FFrameGraphTextureRef sceneDepth,
            RenderCore::FFrameGraphTextureRef    sceneColorHDR,
            Deferred::FDeferredSkyBoxPassInputs& outInputs) -> bool {
            const bool bHasSkyBoxPipeline = (device != nullptr)
                && EnsureFullscreenPipelineFromKeys(*device, resources, resources.SkyBoxVSKey,
                    resources.SkyBoxPSKey, resources.SkyBoxPipelineLayout, resources.SkyBoxPipeline,
                    TEXT("Deferred skybox"), TEXT("BasicDeferred.SkyBoxPipeline"), true);
            if (!viewContext.bHasSkyCube || !viewContext.SkyCubeTexture || !sceneColorHDR.IsValid()
                || !sceneDepth.IsValid() || device == nullptr || !resources.SkyBoxLayout
                || !bHasSkyBoxPipeline || !resources.SkyBoxPipeline || !resources.OutputSampler) {
                return false;
            }

            outInputs.Graph          = &graph;
            outInputs.ViewRect       = &viewRect;
            outInputs.Pipeline       = resources.SkyBoxPipeline.Get();
            outInputs.Layout         = resources.SkyBoxLayout.Get();
            outInputs.Sampler        = resources.OutputSampler.Get();
            outInputs.Bindings       = &resources.SkyBoxBindings;
            outInputs.PerFrameBuffer = perFrameBuffer;
            outInputs.SkyCube        = viewContext.SkyCubeTexture;
            outInputs.SceneDepth     = sceneDepth;
            outInputs.SceneColorHDR  = sceneColorHDR;
            return true;
        }

        [[nodiscard]] auto BuildDefaultPostProcessStack() -> FPostProcessStack {
            FPostProcessStack stack{};
            stack.bEnable = (rPostProcessEnable.GetRenderValue() != 0);

            const bool bEnableTaa = (rPostProcessTaa.GetRenderValue() != 0);
            if (bEnableTaa) {
                FPostProcessNode node{};
                node.EffectId.Assign(TEXT("TAA"));
                node.bEnabled = true;
                node.Params[FString(TEXT("Alpha"))] =
                    FPostProcessParamValue(rPostProcessTaaAlpha.GetRenderValue());
                node.Params[FString(TEXT("ClampK"))] =
                    FPostProcessParamValue(rPostProcessTaaClampK.GetRenderValue());
                stack.Stack.PushBack(Move(node));
            }

            if (rPostProcessBloom.GetRenderValue() != 0) {
                FPostProcessNode node{};
                node.EffectId.Assign(TEXT("Bloom"));
                node.bEnabled = true;
                node.Params[FString(TEXT("Threshold"))] =
                    FPostProcessParamValue(rPostProcessBloomThreshold.GetRenderValue());
                node.Params[FString(TEXT("Knee"))] =
                    FPostProcessParamValue(rPostProcessBloomKnee.GetRenderValue());
                node.Params[FString(TEXT("Intensity"))] =
                    FPostProcessParamValue(rPostProcessBloomIntensity.GetRenderValue());
                node.Params[FString(TEXT("KawaseOffset"))] =
                    FPostProcessParamValue(rPostProcessBloomKawaseOffset.GetRenderValue());
                node.Params[FString(TEXT("Iterations"))] =
                    FPostProcessParamValue(rPostProcessBloomIterations.GetRenderValue());
                node.Params[FString(TEXT("FirstDownsampleLumaWeight"))] = FPostProcessParamValue(
                    rPostProcessBloomFirstDownsampleLumaWeight.GetRenderValue());
                stack.Stack.PushBack(Move(node));
            }

            if (rPostProcessTonemap.GetRenderValue() != 0) {
                FPostProcessNode node{};
                node.EffectId.Assign(TEXT("Tonemap"));
                node.bEnabled                          = true;
                node.Params[FString(TEXT("Exposure"))] = FPostProcessParamValue(1.0f);
                node.Params[FString(TEXT("Gamma"))]    = FPostProcessParamValue(2.2f);
                stack.Stack.PushBack(Move(node));
            }

            if (!bEnableTaa && rPostProcessFxaa.GetRenderValue() != 0) {
                FPostProcessNode node{};
                node.EffectId.Assign(TEXT("Fxaa"));
                node.bEnabled = true;
                node.Params[FString(TEXT("EdgeThreshold"))] =
                    FPostProcessParamValue(rPostProcessFxaaEdgeThreshold.GetRenderValue());
                node.Params[FString(TEXT("EdgeThresholdMin"))] =
                    FPostProcessParamValue(rPostProcessFxaaEdgeThresholdMin.GetRenderValue());
                node.Params[FString(TEXT("Subpix"))] =
                    FPostProcessParamValue(rPostProcessFxaaSubpix.GetRenderValue());
                stack.Stack.PushBack(Move(node));
            }

            return stack;
        }

        void BuildDeferredPerFrameConstants(const RenderCore::View::FViewData& view,
            const RenderCore::Lighting::FLightSceneData*                       lights,
            const Deferred::FCsmBuildResult& csm, FPerFrameConstants& outPerFrameConstants) {
            outPerFrameConstants.ViewProjection = view.Matrices.ViewProjJittered;
            outPerFrameConstants.View           = view.Matrices.View;
            outPerFrameConstants.Proj           = view.Matrices.ProjJittered;
            outPerFrameConstants.ViewProj       = view.Matrices.ViewProjJittered;
            outPerFrameConstants.InvViewProj    = view.Matrices.InvViewProjJittered;

            outPerFrameConstants.ViewOriginWS[0]  = view.ViewOrigin[0];
            outPerFrameConstants.ViewOriginWS[1]  = view.ViewOrigin[1];
            outPerFrameConstants.ViewOriginWS[2]  = view.ViewOrigin[2];
            outPerFrameConstants.bReverseZ        = view.bReverseZ ? 1U : 0U;
            outPerFrameConstants.DebugShadingMode = gDeferredLightingDebugShadingMode;

            const f32 w = static_cast<f32>(view.RenderTargetExtent.Width);
            const f32 h = static_cast<f32>(view.RenderTargetExtent.Height);
            outPerFrameConstants.RenderTargetSize[0]    = w;
            outPerFrameConstants.RenderTargetSize[1]    = h;
            outPerFrameConstants.InvRenderTargetSize[0] = (w > 0.0f) ? (1.0f / w) : 0.0f;
            outPerFrameConstants.InvRenderTargetSize[1] = (h > 0.0f) ? (1.0f / h) : 0.0f;

            RenderCore::Lighting::FDirectionalLight dir{};
            if (lights != nullptr && lights->mHasMainDirectionalLight) {
                dir = lights->mMainDirectionalLight;
            } else {
                dir.mDirectionWS = FVector3f(0.4f, 0.6f, 0.7f);
                dir.mColor       = FVector3f(1.0f, 1.0f, 1.0f);
                dir.mIntensity   = 2.0f;
                dir.mCastShadows = false;
            }

            outPerFrameConstants.DirLightDirectionWS[0] = dir.mDirectionWS[0];
            outPerFrameConstants.DirLightDirectionWS[1] = dir.mDirectionWS[1];
            outPerFrameConstants.DirLightDirectionWS[2] = dir.mDirectionWS[2];
            outPerFrameConstants.DirLightColor[0]       = dir.mColor[0];
            outPerFrameConstants.DirLightColor[1]       = dir.mColor[1];
            outPerFrameConstants.DirLightColor[2]       = dir.mColor[2];
            outPerFrameConstants.DirLightIntensity      = dir.mIntensity;

            outPerFrameConstants.PointLightCount = 0U;
            if (lights != nullptr && !lights->mPointLights.IsEmpty()) {
                const u32 count = static_cast<u32>(lights->mPointLights.Size());
                const u32 clamped =
                    (count > Deferred::kMaxPointLights) ? Deferred::kMaxPointLights : count;
                outPerFrameConstants.PointLightCount = clamped;
                for (u32 i = 0U; i < clamped; ++i) {
                    const auto& src                                   = lights->mPointLights[i];
                    outPerFrameConstants.PointLights[i].PositionWS[0] = src.mPositionWS[0];
                    outPerFrameConstants.PointLights[i].PositionWS[1] = src.mPositionWS[1];
                    outPerFrameConstants.PointLights[i].PositionWS[2] = src.mPositionWS[2];
                    outPerFrameConstants.PointLights[i].Range         = src.mRange;
                    outPerFrameConstants.PointLights[i].Color[0]      = src.mColor[0];
                    outPerFrameConstants.PointLights[i].Color[1]      = src.mColor[1];
                    outPerFrameConstants.PointLights[i].Color[2]      = src.mColor[2];
                    outPerFrameConstants.PointLights[i].Intensity     = src.mIntensity;
                }
            }

            Deferred::FillPerFrameCsmConstants(csm, outPerFrameConstants);
        }

    } // namespace

    auto FBasicDeferredRenderer::GetDefaultMaterialTemplate()
        -> Core::Container::TShared<RenderCore::FMaterialTemplate> {
        auto& resources = GetSharedResources();
        return resources.DefaultTemplate;
    }

    void FBasicDeferredRenderer::SetDefaultMaterialTemplate(
        Core::Container::TShared<RenderCore::FMaterialTemplate> templ) noexcept {
        auto& resources                 = GetSharedResources();
        resources.DefaultTemplate       = Move(templ);
        resources.DefaultPassDesc       = {};
        resources.DefaultShadowPassDesc = {};
        resources.MaterialLayouts.Clear();
        resources.BasePipelineLayouts.Clear();
        resources.BasePipelines.Clear();
        resources.ShadowPipelines.Clear();
    }

    void FBasicDeferredRenderer::SetOutputShaderKeys(
        const RenderCore::FShaderRegistry::FShaderKey& vs,
        const RenderCore::FShaderRegistry::FShaderKey& ps) noexcept {
        auto& resources       = GetSharedResources();
        resources.OutputVSKey = vs;
        resources.OutputPSKey = ps;
        resources.OutputPipeline.Reset();
    }

    void FBasicDeferredRenderer::SetLightingShaderKeys(
        const RenderCore::FShaderRegistry::FShaderKey& vs,
        const RenderCore::FShaderRegistry::FShaderKey& ps) noexcept {
        auto& resources         = GetSharedResources();
        resources.LightingVSKey = vs;
        resources.LightingPSKey = ps;
        resources.LightingPipeline.Reset();
    }

    void FBasicDeferredRenderer::SetSsaoShaderKeys(
        const RenderCore::FShaderRegistry::FShaderKey& vs,
        const RenderCore::FShaderRegistry::FShaderKey& ps) noexcept {
        auto& resources     = GetSharedResources();
        resources.SsaoVSKey = vs;
        resources.SsaoPSKey = ps;
        resources.SsaoPipeline.Reset();
    }

    void FBasicDeferredRenderer::SetSkyBoxShaderKeys(
        const RenderCore::FShaderRegistry::FShaderKey& vs,
        const RenderCore::FShaderRegistry::FShaderKey& ps) noexcept {
        auto& resources       = GetSharedResources();
        resources.SkyBoxVSKey = vs;
        resources.SkyBoxPSKey = ps;
        resources.SkyBoxPipeline.Reset();
    }

    void FBasicDeferredRenderer::SetAtmosphereSkyShaderKeys(
        const RenderCore::FShaderRegistry::FShaderKey& vs,
        const RenderCore::FShaderRegistry::FShaderKey& ps) noexcept {
        auto& resources              = GetSharedResources();
        resources.AtmosphereSkyVSKey = vs;
        resources.AtmosphereSkyPSKey = ps;
        resources.AtmosphereSkyPipeline.Reset();
    }

    auto FBasicDeferredRenderer::RegisterShader(
        const RenderCore::FShaderRegistry::FShaderKey& key, Rhi::FRhiShaderRef shader) -> bool {
        auto& resources = GetSharedResources();
        return resources.Registry.RegisterShader(key, Move(shader));
    }

    void FBasicDeferredRenderer::ShutdownSharedResources() noexcept {
        auto& resources = GetSharedResources();
        resources.OutputPipeline.Reset();
        resources.OutputPipelineLayout.Reset();
        resources.OutputLayout.Reset();
        resources.OutputSampler.Reset();

        resources.LightingPipeline.Reset();
        resources.LightingPipelineLayout.Reset();
        resources.LightingLayout.Reset();
        resources.LightingBindings.Reset();

        resources.SsaoPipeline.Reset();
        resources.SsaoPipelineLayout.Reset();
        resources.SsaoLayout.Reset();
        resources.SsaoBindings.Reset();

        resources.SkyBoxPipeline.Reset();
        resources.SkyBoxPipelineLayout.Reset();
        resources.SkyBoxLayout.Reset();
        resources.SkyBoxBindings.Reset();

        resources.AtmosphereSkyPipeline.Reset();
        resources.AtmosphereSkyPipelineLayout.Reset();
        resources.AtmosphereSkyLayout.Reset();
        resources.AtmosphereSkyBindings.Reset();

        resources.IblBlackCube.Reset();
        resources.IblBlack2D.Reset();
        resources.ShadowMapCSM.Reset();
        resources.ShadowMapCSMSize   = 0U;
        resources.ShadowMapCSMLayers = 0U;

        resources.PerFrameLayout.Reset();
        resources.PerDrawLayout.Reset();
        resources.PerFrameBinding = 0U;
        resources.PerDrawBinding  = 0U;

        resources.BasePipelines.Clear();
        resources.ShadowPipelines.Clear();
        resources.MaterialLayouts.Clear();
        resources.BasePipelineLayouts.Clear();
        resources.DefaultTemplate.Reset();
        resources.DefaultPassDesc       = {};
        resources.DefaultShadowPassDesc = {};

        resources.OutputVSKey        = {};
        resources.OutputPSKey        = {};
        resources.LightingVSKey      = {};
        resources.LightingPSKey      = {};
        resources.SsaoVSKey          = {};
        resources.SsaoPSKey          = {};
        resources.SkyBoxVSKey        = {};
        resources.SkyBoxPSKey        = {};
        resources.AtmosphereSkyVSKey = {};
        resources.AtmosphereSkyPSKey = {};

        resources.Registry.Clear();
    }

    void FBasicDeferredRenderer::SetDeferredLightingDebugLambert(bool bEnabled) noexcept {
        gDeferredLightingDebugShadingMode = bEnabled ? 1U : 0U;
    }

    void FBasicDeferredRenderer::SetViewContext(const FRenderViewContext& context) {
        FBaseRenderer::SetViewContext(context);
        mGraphOutputs = {};
    }

    void FBasicDeferredRenderer::RegisterBuiltinPasses() {
        FRendererPassRegistration gbufferPass{};
        gbufferPass.mPassId.Assign(TEXT("Deferred.GBufferBase"));
        gbufferPass.mPassSet = ERendererPassSet::DeferredBase;
        gbufferPass.mExecute = [this](RenderCore::FFrameGraph& graph) {
            RegisterDeferredGBufferBasePass(graph);
        };
        RegisterPassToSet(gbufferPass);

        FRendererPassRegistration shadowPass{};
        shadowPass.mPassId.Assign(TEXT("Deferred.CsmShadow"));
        shadowPass.mPassSet = ERendererPassSet::Shadow;
        shadowPass.mExecute = [this](RenderCore::FFrameGraph& graph) {
            RegisterDeferredShadowPass(graph);
        };
        RegisterPassToSet(shadowPass);

        FRendererPassRegistration ssaoPass{};
        ssaoPass.mPassId.Assign(TEXT("Deferred.Ssao"));
        ssaoPass.mPassSet     = ERendererPassSet::DeferredBase;
        ssaoPass.mAnchorOrder = ERendererPassAnchorOrder::After;
        ssaoPass.mAnchorPassId.Assign(TEXT("Deferred.GBufferBase"));
        ssaoPass.mExecute = [this](RenderCore::FFrameGraph& graph) {
            RegisterDeferredSsaoPass(graph);
        };
        RegisterPassToSet(ssaoPass);

        FRendererPassRegistration lightingPass{};
        lightingPass.mPassId.Assign(TEXT("Deferred.Lighting"));
        lightingPass.mPassSet     = ERendererPassSet::DeferredBase;
        lightingPass.mAnchorOrder = ERendererPassAnchorOrder::After;
        lightingPass.mAnchorPassId.Assign(TEXT("Deferred.Ssao"));
        lightingPass.mExecute = [this](RenderCore::FFrameGraph& graph) {
            RegisterDeferredLightingPass(graph);
        };
        RegisterPassToSet(lightingPass);

        FRendererPassRegistration skyPass{};
        skyPass.mPassId.Assign(TEXT("Deferred.Sky"));
        skyPass.mPassSet     = ERendererPassSet::DeferredBase;
        skyPass.mAnchorOrder = ERendererPassAnchorOrder::After;
        skyPass.mAnchorPassId.Assign(TEXT("Deferred.Lighting"));
        skyPass.mExecute = [this](
                               RenderCore::FFrameGraph& graph) { RegisterDeferredSkyPass(graph); };
        RegisterPassToSet(skyPass);

        FRendererPassRegistration postProcessPass{};
        postProcessPass.mPassId.Assign(TEXT("Deferred.PostProcess"));
        postProcessPass.mPassSet = ERendererPassSet::PostProcess;
        postProcessPass.mExecute = [this](RenderCore::FFrameGraph& graph) {
            RegisterDeferredPostProcessPass(graph);
        };
        RegisterPassToSet(postProcessPass);
    }

    void FBasicDeferredRenderer::PrepareForRendering(Rhi::FRhiDevice& device) {
        auto& resources = GetSharedResources();
        EnsureDefaultTemplate(resources);
        EnsureVertexLayout(resources);
        EnsureLayouts(device, resources);
        const bool bHasLightingPipeline = EnsureFullscreenPipelineFromKeys(device, resources,
            resources.LightingVSKey, resources.LightingPSKey, resources.LightingPipelineLayout,
            resources.LightingPipeline, TEXT("Deferred lighting"),
            TEXT("BasicDeferred.DeferredLightingPipeline"), false);
        if (!bHasLightingPipeline) {
            Assert(false, TEXT("BasicDeferredRenderer"),
                "PrepareForRendering failed: lighting pipeline is unavailable.");
            return;
        }
        const bool bHasSsaoPipeline =
            EnsureFullscreenPipelineFromKeys(device, resources, resources.SsaoVSKey,
                resources.SsaoPSKey, resources.SsaoPipelineLayout, resources.SsaoPipeline,
                TEXT("Deferred SSAO"), TEXT("BasicDeferred.SsaoPipeline"), false);
        if (!bHasSsaoPipeline) {
            Assert(false, TEXT("BasicDeferredRenderer"),
                "PrepareForRendering failed: SSAO pipeline is unavailable.");
            return;
        }

        if (!mPerFrameBuffer) {
            Rhi::FRhiBufferDesc desc{};
            desc.mDebugName.Assign(TEXT("Deferred.PerFrame"));
            desc.mSizeBytes = sizeof(FPerFrameConstants);
            desc.mUsage     = Rhi::ERhiResourceUsage::Dynamic;
            desc.mBindFlags = Rhi::ERhiBufferBindFlags::Constant;
            desc.mCpuAccess = Rhi::ERhiCpuAccess::Write;
            mPerFrameBuffer = device.CreateBuffer(desc);
        }

        if (!mIblConstantsBuffer) {
            Rhi::FRhiBufferDesc desc{};
            desc.mDebugName.Assign(TEXT("Deferred.IBLConstants"));
            desc.mSizeBytes     = sizeof(FIblConstants);
            desc.mUsage         = Rhi::ERhiResourceUsage::Dynamic;
            desc.mBindFlags     = Rhi::ERhiBufferBindFlags::Constant;
            desc.mCpuAccess     = Rhi::ERhiCpuAccess::Write;
            mIblConstantsBuffer = device.CreateBuffer(desc);
        }

        struct FSsaoConstants {
            u32 Enable      = 1U;
            u32 SampleCount = 12U;
            f32 RadiusVS    = 0.55f;
            f32 BiasNdc     = 0.0005f;
            f32 Power       = 1.6f;
            f32 Intensity   = 1.0f;
            f32 _pad0       = 0.0f;
            f32 _pad1       = 0.0f;
        };

        if (!mSsaoConstantsBuffer) {
            Rhi::FRhiBufferDesc desc{};
            desc.mDebugName.Assign(TEXT("Deferred.SsaoConstants"));
            desc.mSizeBytes      = sizeof(FSsaoConstants);
            desc.mUsage          = Rhi::ERhiResourceUsage::Dynamic;
            desc.mBindFlags      = Rhi::ERhiBufferBindFlags::Constant;
            desc.mCpuAccess      = Rhi::ERhiCpuAccess::Write;
            mSsaoConstantsBuffer = device.CreateBuffer(desc);
        }

        if (!mPerFrameGroup && mPerFrameBuffer && resources.PerFrameLayout) {
            RenderCore::ShaderBinding::FBindGroupBuilder builder(resources.PerFrameLayout.Get());
            DebugAssert(builder.AddBuffer(resources.PerFrameBinding, mPerFrameBuffer.Get(), 0ULL,
                            static_cast<u64>(sizeof(FPerFrameConstants))),
                TEXT("BasicDeferredRenderer"), "Failed to add per-frame binding (binding={}).",
                resources.PerFrameBinding);
            Rhi::FRhiBindGroupDesc groupDesc{};
            DebugAssert(builder.Build(groupDesc), TEXT("BasicDeferredRenderer"),
                "Failed to build per-frame bind group desc from layout.");
            mPerFrameGroup = device.CreateBindGroup(groupDesc);
        }

        // Shadow per-cascade per-frame buffers (avoid "last update wins" on deferred contexts).
        if (resources.PerFrameLayout) {
            for (u32 i = 0U; i < kShadowCascades; ++i) {
                if (!mShadowPerFrameBuffers[i]) {
                    Rhi::FRhiBufferDesc desc{};
                    desc.mDebugName.Assign(TEXT("Deferred.Shadow.PerFrame"));
                    desc.mSizeBytes           = sizeof(FPerFrameConstants);
                    desc.mUsage               = Rhi::ERhiResourceUsage::Dynamic;
                    desc.mBindFlags           = Rhi::ERhiBufferBindFlags::Constant;
                    desc.mCpuAccess           = Rhi::ERhiCpuAccess::Write;
                    mShadowPerFrameBuffers[i] = device.CreateBuffer(desc);
                }

                if (!mShadowPerFrameGroups[i] && mShadowPerFrameBuffers[i]) {
                    RenderCore::ShaderBinding::FBindGroupBuilder builder(
                        resources.PerFrameLayout.Get());
                    DebugAssert(builder.AddBuffer(resources.PerFrameBinding,
                                    mShadowPerFrameBuffers[i].Get(), 0ULL,
                                    static_cast<u64>(sizeof(FPerFrameConstants))),
                        TEXT("BasicDeferredRenderer"),
                        "Failed to add shadow per-frame binding (binding={}, cascade={}).",
                        resources.PerFrameBinding, i);
                    Rhi::FRhiBindGroupDesc groupDesc{};
                    DebugAssert(builder.Build(groupDesc), TEXT("BasicDeferredRenderer"),
                        "Failed to build shadow per-frame bind group desc from layout.");
                    mShadowPerFrameGroups[i] = device.CreateBindGroup(groupDesc);
                }
            }
        }

        mPerDrawStrideBytes = static_cast<u32>(sizeof(FInstanceDrawData));
        mPerDrawCapacity    = GetPerDrawInstanceCapacity();

        const u64 perDrawBufferSize = static_cast<u64>(mPerDrawStrideBytes)
            * static_cast<u64>(mPerDrawCapacity)
            * static_cast<u64>(IsVulkanBackend() ? FBasicDeferredRenderer::kInstanceFrameRing : 1U);

        if (!mPerDrawBuffer || mPerDrawBuffer->GetDesc().mSizeBytes != perDrawBufferSize) {
            for (auto& group : mPerDrawGroups) {
                group.Reset();
            }
            mPerDrawBuffer.Reset();

            Rhi::FRhiBufferDesc desc{};
            desc.mDebugName.Assign(TEXT("Deferred.PerDraw"));
            desc.mSizeBytes = perDrawBufferSize;
            desc.mUsage     = IsVulkanBackend() ? Rhi::ERhiResourceUsage::Dynamic
                                                : Rhi::ERhiResourceUsage::Default;
            desc.mBindFlags = Rhi::ERhiBufferBindFlags::ShaderResource;
            desc.mCpuAccess =
                IsVulkanBackend() ? Rhi::ERhiCpuAccess::Write : Rhi::ERhiCpuAccess::None;
            mPerDrawBuffer = device.CreateBuffer(desc);
        }

        if (!mPerDrawGroups[0] && mPerDrawBuffer && resources.PerDrawLayout) {
            RebuildPerDrawBindGroups(device, resources, mPerDrawBuffer.Get(), mPerDrawStrideBytes,
                mPerDrawCapacity, mPerDrawGroups);
        }
    }
    void FBasicDeferredRenderer::RegisterDeferredGBufferBasePass(RenderCore::FFrameGraph& graph) {
        ResetBasePassPipelineStats();

        const auto* view         = mViewContext.View;
        auto*       outputTarget = mViewContext.OutputTarget;
        const auto* drawList     = mViewContext.DrawList;
        if (view == nullptr || outputTarget == nullptr) {
            Assert(false, TEXT("BasicDeferredRenderer"),
                "Render skipped: view/output target is null (view={}, outputTarget={}).",
                static_cast<int>(view ? 1 : 0), static_cast<int>(outputTarget ? 1 : 0));
            return;
        }

        Assert(view->IsValid(), TEXT("BasicDeferredRenderer"), "Render skipped: view is invalid.");

        const u32 width  = view->RenderTargetExtent.Width;
        const u32 height = view->RenderTargetExtent.Height;

        Assert(width > 0U && height > 0U, TEXT("BasicDeferredRenderer"),
            "Render skipped: render extent is invalid {}x{}.", static_cast<u32>(width),
            static_cast<u32>(height));

        DebugAssert(mPerDrawStrideBytes == static_cast<u32>(sizeof(FInstanceDrawData)),
            TEXT("BasicDeferredRenderer"),
            "PerDraw instance stride invalid (stride={}, expected={}).", mPerDrawStrideBytes,
            static_cast<u32>(sizeof(FInstanceDrawData)));
        DebugAssert(mPerDrawCapacity > 0U, TEXT("BasicDeferredRenderer"),
            "PerDraw instance capacity invalid (capacity={}).", mPerDrawCapacity);

        // TODO: Refactor: to `mPerDrawFrameSlot` should be an attribute of `RenderCore` module
        mPerDrawFrameSlot =
            IsVulkanBackend() ? static_cast<u32>(view->FrameIndex % kInstanceFrameRing) : 0U;

        if (mPerFrameBuffer) {
            FPerFrameConstants constants{};
            constants.ViewProjection = view->Matrices.ViewProjJittered;
            UpdateConstantBuffer(mPerFrameBuffer.Get(), &constants, sizeof(constants));
        }

        constexpr Rhi::FRhiClearColor kAlbedoClear{ 0.12f, 0.12f, 0.12f, 1.0f };
        constexpr Rhi::FRhiClearColor kNormalClear{ 0.5f, 0.5f, 1.0f, 1.0f };
        constexpr Rhi::FRhiClearColor kEmissiveClear{ 0.0f, 0.0f, 0.0f, 1.0f };

        struct FBasePassData {
            RenderCore::FFrameGraphTextureRef GBufferA;
            RenderCore::FFrameGraphTextureRef GBufferB;
            RenderCore::FFrameGraphTextureRef GBufferC;
            RenderCore::FFrameGraphTextureRef Depth;
            RenderCore::FFrameGraphRTVRef     GBufferARTV;
            RenderCore::FFrameGraphRTVRef     GBufferBRTV;
            RenderCore::FFrameGraphRTVRef     GBufferCRTV;
            RenderCore::FFrameGraphDSVRef     DepthDSV;
        };

        RenderCore::FFrameGraphPassDesc basePassDesc{};
        basePassDesc.mName  = "BasicDeferred.BasePass";
        basePassDesc.mType  = RenderCore::EFrameGraphPassType::Raster;
        basePassDesc.mQueue = RenderCore::EFrameGraphQueue::Graphics;

        auto&       resources = GetSharedResources();
        auto*       device    = Rhi::RHIGetDevice();
        const auto* lights    = mViewContext.Lights;
        Assert(device != nullptr, TEXT("BasicDeferredRenderer"),
            "Render failed: RHI device is null while preparing instance buffer.");

        Deferred::FCsmBuildInputs csmInputs{};
        csmInputs.View                      = view;
        csmInputs.Lights                    = lights;
        const Deferred::FCsmBuildResult csm = Deferred::BuildCsm(csmInputs);

        u32                             requiredPerDrawInstances = 0U;
        const u32                       basePassInstanceCount    = CountDrawListInstances(drawList);
        requiredPerDrawInstances += basePassInstanceCount;
        u32 accumulatedFirstInstance = basePassInstanceCount;
        for (u32 cascadeIndex = 0U; cascadeIndex < csm.Data.mCascadeCount
            && cascadeIndex < RenderCore::Shadow::kMaxCascades;
            ++cascadeIndex) {
            const u32 cascadeInstanceCount =
                CountDrawListInstances(mViewContext.ShadowDrawLists[cascadeIndex]);
            requiredPerDrawInstances += cascadeInstanceCount;
            accumulatedFirstInstance += cascadeInstanceCount;
        }
        DebugAssert(accumulatedFirstInstance == requiredPerDrawInstances,
            TEXT("BasicDeferredRenderer"),
            "PerDraw instance accumulation mismatch (accumulated={}, required={}).",
            accumulatedFirstInstance, requiredPerDrawInstances);
        if (requiredPerDrawInstances > mPerDrawCapacity) {
            u32 grownCapacity = (mPerDrawCapacity > 0U) ? mPerDrawCapacity : 1U;
            while (grownCapacity < requiredPerDrawInstances) {
                grownCapacity *= 2U;
            }

            mPerDrawCapacity = grownCapacity;
            for (auto& group : mPerDrawGroups) {
                group.Reset();
            }
            mPerDrawBuffer.Reset();

            Rhi::FRhiBufferDesc desc{};
            desc.mDebugName.Assign(TEXT("Deferred.PerDraw"));
            desc.mSizeBytes = static_cast<u64>(mPerDrawStrideBytes)
                * static_cast<u64>(mPerDrawCapacity)
                * static_cast<u64>(IsVulkanBackend() ? kInstanceFrameRing : 1U);
            desc.mUsage     = IsVulkanBackend() ? Rhi::ERhiResourceUsage::Dynamic
                                                : Rhi::ERhiResourceUsage::Default;
            desc.mBindFlags = Rhi::ERhiBufferBindFlags::ShaderResource;
            desc.mCpuAccess =
                IsVulkanBackend() ? Rhi::ERhiCpuAccess::Write : Rhi::ERhiCpuAccess::None;
            mPerDrawBuffer = device->CreateBuffer(desc);

            if (mPerDrawBuffer && resources.PerDrawLayout) {
                RebuildPerDrawBindGroups(*device, resources, mPerDrawBuffer.Get(),
                    mPerDrawStrideBytes, mPerDrawCapacity, mPerDrawGroups);
            }
        }
        DebugAssert(requiredPerDrawInstances <= mPerDrawCapacity, TEXT("BasicDeferredRenderer"),
            "PerDraw instance capacity too small for frame (required={}, capacity={}).",
            requiredPerDrawInstances, mPerDrawCapacity);

        FDrawListBindings drawBindings{};
        drawBindings.PerDraw  = mPerDrawGroups[mPerDrawFrameSlot].Get();
        drawBindings.PerFrame = mPerFrameGroup.Get();
        drawBindings.PerFrameSetIndex =
            resources.PerFrameLayout ? resources.PerFrameLayout->GetDesc().mSetIndex : 0U;
        drawBindings.PerDrawSetIndex =
            resources.PerDrawLayout ? resources.PerDrawLayout->GetDesc().mSetIndex : 0U;
        drawBindings.PerMaterialSetIndex  = IsVulkanBackend() ? 2U : 0U;
        drawBindings.ResolvedVertexLayout = &resources.BaseVertexLayout;
        DebugAssert(drawBindings.PerFrame != nullptr, TEXT("BasicDeferredRenderer"),
            "Per-frame bind group is null for base pass.");
        DebugAssert(drawBindings.PerDraw != nullptr, TEXT("BasicDeferredRenderer"),
            "Per-draw bind group is null for base pass.");

        FBasePassPipelineData pipelineData{};
        pipelineData.Device          = device;
        pipelineData.Registry        = &resources.Registry;
        pipelineData.PipelineCache   = &resources.BasePipelines;
        pipelineData.DefaultPassDesc = &resources.DefaultPassDesc;
        pipelineData.VertexLayout    = resources.BaseVertexLayout;

        FBasePassBindingData bindingData{};
        bindingData.PerDrawBuffer          = mPerDrawBuffer.Get();
        bindingData.PerDrawStrideBytes     = mPerDrawStrideBytes;
        bindingData.PerDrawCapacity        = mPerDrawCapacity;
        bindingData.PerDrawBaseOffsetBytes = static_cast<u64>(mPerDrawFrameSlot)
            * static_cast<u64>(mPerDrawCapacity) * static_cast<u64>(mPerDrawStrideBytes);
        bindingData.PerDrawCursorInstances = 0U;
        bindingData.bUseInstanceCursor     = IsVulkanBackend();
        if (bindingData.PerDrawBuffer != nullptr) {
            const u64 requiredBytes = bindingData.PerDrawBaseOffsetBytes
                + static_cast<u64>(bindingData.PerDrawCapacity)
                    * static_cast<u64>(bindingData.PerDrawStrideBytes);
            DebugAssert(requiredBytes <= bindingData.PerDrawBuffer->GetDesc().mSizeBytes,
                TEXT("BasicDeferredRenderer"), "PerDraw buffer too small (required={}, actual={}).",
                requiredBytes, bindingData.PerDrawBuffer->GetDesc().mSizeBytes);
        }

        const RenderCore::View::FViewRect viewRect = view->ViewRect;

        graph.AddPass<FBasePassData>(
            basePassDesc,
            [&](RenderCore::FFrameGraphPassBuilder& builder, FBasePassData& data) -> void {
                RenderCore::FFrameGraphTextureDesc gbufferADesc{};
                gbufferADesc.mDesc.mDebugName.Assign(TEXT("GBufferA.Albedo"));
                gbufferADesc.mDesc.mWidth     = width;
                gbufferADesc.mDesc.mHeight    = height;
                gbufferADesc.mDesc.mFormat    = Rhi::ERhiFormat::R8G8B8A8Unorm;
                gbufferADesc.mDesc.mBindFlags = Rhi::ERhiTextureBindFlags::RenderTarget
                    | Rhi::ERhiTextureBindFlags::ShaderResource;

                RenderCore::FFrameGraphTextureDesc gbufferBDesc{};
                gbufferBDesc.mDesc.mDebugName.Assign(TEXT("GBufferB.Normal"));
                gbufferBDesc.mDesc.mWidth     = width;
                gbufferBDesc.mDesc.mHeight    = height;
                gbufferBDesc.mDesc.mFormat    = Rhi::ERhiFormat::R16G16B16A16Float;
                gbufferBDesc.mDesc.mBindFlags = Rhi::ERhiTextureBindFlags::RenderTarget
                    | Rhi::ERhiTextureBindFlags::ShaderResource;

                RenderCore::FFrameGraphTextureDesc gbufferCDesc{};
                gbufferCDesc.mDesc.mDebugName.Assign(TEXT("GBufferC.Emissive"));
                gbufferCDesc.mDesc.mWidth     = width;
                gbufferCDesc.mDesc.mHeight    = height;
                gbufferCDesc.mDesc.mFormat    = Rhi::ERhiFormat::R8G8B8A8Unorm;
                gbufferCDesc.mDesc.mBindFlags = Rhi::ERhiTextureBindFlags::RenderTarget
                    | Rhi::ERhiTextureBindFlags::ShaderResource;

                RenderCore::FFrameGraphTextureDesc depthDesc{};
                depthDesc.mDesc.mDebugName.Assign(TEXT("GBufferDepth"));
                depthDesc.mDesc.mWidth     = width;
                depthDesc.mDesc.mHeight    = height;
                depthDesc.mDesc.mFormat    = Rhi::ERhiFormat::D32Float;
                depthDesc.mDesc.mBindFlags = Rhi::ERhiTextureBindFlags::DepthStencil
                    | Rhi::ERhiTextureBindFlags::ShaderResource;

                data.GBufferA = builder.CreateTexture(gbufferADesc);
                data.GBufferB = builder.CreateTexture(gbufferBDesc);
                data.GBufferC = builder.CreateTexture(gbufferCDesc);
                data.Depth    = builder.CreateTexture(depthDesc);

                data.GBufferA = builder.Write(data.GBufferA, Rhi::ERhiResourceState::RenderTarget);
                data.GBufferB = builder.Write(data.GBufferB, Rhi::ERhiResourceState::RenderTarget);
                data.GBufferC = builder.Write(data.GBufferC, Rhi::ERhiResourceState::RenderTarget);
                data.Depth    = builder.Write(data.Depth, Rhi::ERhiResourceState::DepthWrite);

                if (drawList != nullptr) {
                    for (const auto& bucket : drawList->mBuckets) {
                        const auto* material = bucket.mMaterial;
                        if (material == nullptr) {
                            continue;
                        }
                        const auto* layout = material->FindLayout(bucket.mPass);
                        if (layout == nullptr) {
                            continue;
                        }
                        const auto& parameters = material->GetParameters();
                        for (const auto paramId : layout->mTextureNameHashes) {
                            const auto* textureParam = parameters.FindTextureParam(paramId);
                            if (textureParam == nullptr || !textureParam->mSrv) {
                                continue;
                            }
                            const auto& textureRef = textureParam->mSrv->GetTextureRef();
                            if (!textureRef) {
                                continue;
                            }
                            const auto importedRef =
                                graph.ImportTexture(textureRef, Rhi::ERhiResourceState::Common);
                            if (importedRef.IsValid()) {
                                builder.Read(importedRef, Rhi::ERhiResourceState::ShaderResource);
                            }
                        }
                    }
                }

                Rhi::FRhiTextureViewRange viewRange{};
                viewRange.mMipCount        = 1U;
                viewRange.mLayerCount      = 1U;
                viewRange.mDepthSliceCount = 1U;

                Rhi::FRhiRenderTargetViewDesc rtvDescA{};
                rtvDescA.mDebugName.Assign(TEXT("GBufferA.RTV"));
                rtvDescA.mFormat = gbufferADesc.mDesc.mFormat;
                rtvDescA.mRange  = viewRange;
                data.GBufferARTV = builder.CreateRTV(data.GBufferA, rtvDescA);

                Rhi::FRhiRenderTargetViewDesc rtvDescB{};
                rtvDescB.mDebugName.Assign(TEXT("GBufferB.RTV"));
                rtvDescB.mFormat = gbufferBDesc.mDesc.mFormat;
                rtvDescB.mRange  = viewRange;
                data.GBufferBRTV = builder.CreateRTV(data.GBufferB, rtvDescB);

                Rhi::FRhiRenderTargetViewDesc rtvDescC{};
                rtvDescC.mDebugName.Assign(TEXT("GBufferC.RTV"));
                rtvDescC.mFormat = gbufferCDesc.mDesc.mFormat;
                rtvDescC.mRange  = viewRange;
                data.GBufferCRTV = builder.CreateRTV(data.GBufferC, rtvDescC);

                Rhi::FRhiDepthStencilViewDesc dsvDesc{};
                dsvDesc.mDebugName.Assign(TEXT("GBufferDepth.DSV"));
                dsvDesc.mFormat = depthDesc.mDesc.mFormat;
                dsvDesc.mRange  = viewRange;
                data.DepthDSV   = builder.CreateDSV(data.Depth, dsvDesc);

                RenderCore::FRdgRenderTargetBinding rtvs[3]{};
                rtvs[0].mRTV        = data.GBufferARTV;
                rtvs[0].mLoadOp     = Rhi::ERhiLoadOp::Clear;
                rtvs[0].mStoreOp    = Rhi::ERhiStoreOp::Store;
                rtvs[0].mClearColor = kAlbedoClear;

                rtvs[1].mRTV        = data.GBufferBRTV;
                rtvs[1].mLoadOp     = Rhi::ERhiLoadOp::Clear;
                rtvs[1].mStoreOp    = Rhi::ERhiStoreOp::Store;
                rtvs[1].mClearColor = kNormalClear;

                rtvs[2].mRTV        = data.GBufferCRTV;
                rtvs[2].mLoadOp     = Rhi::ERhiLoadOp::Clear;
                rtvs[2].mStoreOp    = Rhi::ERhiStoreOp::Store;
                rtvs[2].mClearColor = kEmissiveClear;

                RenderCore::FRdgDepthStencilBinding depthBinding{};
                depthBinding.mDSV          = data.DepthDSV;
                depthBinding.mDepthLoadOp  = Rhi::ERhiLoadOp::Clear;
                depthBinding.mDepthStoreOp = Rhi::ERhiStoreOp::Store;
                // Reverse-Z view clears depth to 0.0f (far), non-reversed clears to 1.0f.
                depthBinding.mClearDepthStencil.mDepth =
                    (view != nullptr && view->bReverseZ) ? 0.0f : 1.0f;

                builder.SetRenderTargets(rtvs, 3U, &depthBinding);

                mGraphOutputs.mGBufferA   = data.GBufferA;
                mGraphOutputs.mGBufferB   = data.GBufferB;
                mGraphOutputs.mGBufferC   = data.GBufferC;
                mGraphOutputs.mSceneDepth = data.Depth;
            },
            [drawList, drawBindings, pipelineData, bindingData, viewRect](Rhi::FRhiCmdContext& ctx,
                const RenderCore::FFrameGraphPassResources&, const FBasePassData&) -> void {
                Rhi::FRhiDebugMarker  marker(ctx, TEXT("Deferred.BasePass"));
                Rhi::FRhiViewportRect viewport{};
                viewport.mX        = static_cast<f32>(viewRect.X);
                viewport.mY        = static_cast<f32>(viewRect.Y);
                viewport.mWidth    = static_cast<f32>(viewRect.Width);
                viewport.mHeight   = static_cast<f32>(viewRect.Height);
                viewport.mMinDepth = 0.0f;
                viewport.mMaxDepth = 1.0f;
                ctx.RHISetViewport(viewport);

                Rhi::FRhiScissorRect scissor{};
                scissor.mX      = viewRect.X;
                scissor.mY      = viewRect.Y;
                scissor.mWidth  = viewRect.Width;
                scissor.mHeight = viewRect.Height;
                ctx.RHISetScissor(scissor);

                if (drawList != nullptr) {
                    FDrawListExecutor::ExecuteBasePass(ctx, *drawList, drawBindings,
                        ResolveBasePassPipeline, const_cast<FBasePassPipelineData*>(&pipelineData),
                        BindPerDraw, const_cast<FBasePassBindingData*>(&bindingData));
                }
            });
    }

    void FBasicDeferredRenderer::RegisterDeferredShadowPass(RenderCore::FFrameGraph& graph) {
        const auto* view         = mViewContext.View;
        auto*       outputTarget = mViewContext.OutputTarget;
        const auto* drawList     = mViewContext.DrawList;
        if (view == nullptr || outputTarget == nullptr) {
            Assert(false, TEXT("BasicDeferredRenderer"),
                "Render skipped: view/output target is null (view={}, outputTarget={}).",
                static_cast<int>(view ? 1 : 0), static_cast<int>(outputTarget ? 1 : 0));
            return;
        }

        Assert(view->IsValid(), TEXT("BasicDeferredRenderer"), "Render skipped: view is invalid.");
        const u32 width  = view->RenderTargetExtent.Width;
        const u32 height = view->RenderTargetExtent.Height;
        Assert(width > 0U && height > 0U, TEXT("BasicDeferredRenderer"),
            "Render skipped: render extent is invalid {}x{}.", static_cast<u32>(width),
            static_cast<u32>(height));

        auto&       resources = GetSharedResources();
        auto*       device    = Rhi::RHIGetDevice();
        const auto* lights    = mViewContext.Lights;
        Assert(device != nullptr, TEXT("BasicDeferredRenderer"),
            "Render failed: RHI device is null while preparing shadow pass.");

        Deferred::FCsmBuildInputs csmInputs{};
        csmInputs.View                      = view;
        csmInputs.Lights                    = lights;
        const Deferred::FCsmBuildResult csm = Deferred::BuildCsm(csmInputs);

        const u32                       basePassInstanceCount    = CountDrawListInstances(drawList);
        u32                             requiredPerDrawInstances = basePassInstanceCount;
        TArray<u32, 4U>                 shadowPassFirstInstance{};
        u32                             accumulatedFirstInstance = basePassInstanceCount;
        for (u32 cascadeIndex = 0U; cascadeIndex < csm.Data.mCascadeCount
            && cascadeIndex < RenderCore::Shadow::kMaxCascades;
            ++cascadeIndex) {
            shadowPassFirstInstance[cascadeIndex] = accumulatedFirstInstance;
            const u32 cascadeInstanceCount =
                CountDrawListInstances(mViewContext.ShadowDrawLists[cascadeIndex]);
            requiredPerDrawInstances += cascadeInstanceCount;
            accumulatedFirstInstance += cascadeInstanceCount;
        }
        if (requiredPerDrawInstances > mPerDrawCapacity) {
            u32 grownCapacity = (mPerDrawCapacity > 0U) ? mPerDrawCapacity : 1U;
            while (grownCapacity < requiredPerDrawInstances) {
                grownCapacity *= 2U;
            }

            mPerDrawCapacity = grownCapacity;
            for (auto& group : mPerDrawGroups) {
                group.Reset();
            }
            mPerDrawBuffer.Reset();

            Rhi::FRhiBufferDesc desc{};
            desc.mDebugName.Assign(TEXT("Deferred.PerDraw"));
            desc.mSizeBytes = static_cast<u64>(mPerDrawStrideBytes)
                * static_cast<u64>(mPerDrawCapacity)
                * static_cast<u64>(IsVulkanBackend() ? kInstanceFrameRing : 1U);
            desc.mUsage     = IsVulkanBackend() ? Rhi::ERhiResourceUsage::Dynamic
                                                : Rhi::ERhiResourceUsage::Default;
            desc.mBindFlags = Rhi::ERhiBufferBindFlags::ShaderResource;
            desc.mCpuAccess =
                IsVulkanBackend() ? Rhi::ERhiCpuAccess::Write : Rhi::ERhiCpuAccess::None;
            mPerDrawBuffer = device->CreateBuffer(desc);
            if (mPerDrawBuffer && resources.PerDrawLayout) {
                RebuildPerDrawBindGroups(*device, resources, mPerDrawBuffer.Get(),
                    mPerDrawStrideBytes, mPerDrawCapacity, mPerDrawGroups);
            }
        }

        FDrawListBindings drawBindings{};
        drawBindings.PerDraw  = mPerDrawGroups[mPerDrawFrameSlot].Get();
        drawBindings.PerFrame = mPerFrameGroup.Get();
        drawBindings.PerFrameSetIndex =
            resources.PerFrameLayout ? resources.PerFrameLayout->GetDesc().mSetIndex : 0U;
        drawBindings.PerDrawSetIndex =
            resources.PerDrawLayout ? resources.PerDrawLayout->GetDesc().mSetIndex : 0U;
        drawBindings.PerMaterialSetIndex  = IsVulkanBackend() ? 2U : 0U;
        drawBindings.ResolvedVertexLayout = &resources.BaseVertexLayout;

        FBasePassPipelineData pipelineData{};
        pipelineData.Device          = device;
        pipelineData.Registry        = &resources.Registry;
        pipelineData.PipelineCache   = &resources.BasePipelines;
        pipelineData.DefaultPassDesc = &resources.DefaultPassDesc;
        pipelineData.VertexLayout    = resources.BaseVertexLayout;

        FBasePassBindingData bindingData{};
        bindingData.PerDrawBuffer          = mPerDrawBuffer.Get();
        bindingData.PerDrawStrideBytes     = mPerDrawStrideBytes;
        bindingData.PerDrawCapacity        = mPerDrawCapacity;
        bindingData.PerDrawBaseOffsetBytes = static_cast<u64>(mPerDrawFrameSlot)
            * static_cast<u64>(mPerDrawCapacity) * static_cast<u64>(mPerDrawStrideBytes);
        bindingData.PerDrawCursorInstances = 0U;
        bindingData.bUseInstanceCursor     = IsVulkanBackend();

        FBasePassPipelineData shadowPipelineData = pipelineData;
        shadowPipelineData.DefaultPassDesc       = &resources.DefaultShadowPassDesc;

        Shadowing::FDeferredCsmPassSetInputs<FBasePassPipelineData, FBasePassBindingData>
            shadowInputs{};
        shadowInputs.mGraph                     = &graph;
        shadowInputs.mView                      = view;
        shadowInputs.mCsm                       = &csm;
        shadowInputs.mDrawBindings              = drawBindings;
        shadowInputs.mShadowPipelineData        = shadowPipelineData;
        shadowInputs.mFallbackBindingData       = bindingData;
        shadowInputs.mFallbackPerFrameBuffer    = mPerFrameBuffer.Get();
        shadowInputs.mFallbackPerFrameGroup     = mPerFrameGroup.Get();
        shadowInputs.mPersistentShadowMap       = &resources.ShadowMapCSM;
        shadowInputs.mPersistentShadowMapSize   = &resources.ShadowMapCSMSize;
        shadowInputs.mPersistentShadowMapLayers = &resources.ShadowMapCSMLayers;
        shadowInputs.mResolveShadowPipeline     = ResolveShadowPassPipeline;
        shadowInputs.mBindPerDraw               = BindPerDraw;
        for (u32 i = 0U; i < 4U; ++i) {
            shadowInputs.mShadowDrawLists[i]       = mViewContext.ShadowDrawLists[i];
            shadowInputs.mBindingDataPerCascade[i] = bindingData;
            shadowInputs.mBindingDataPerCascade[i].PerDrawCursorInstances =
                bindingData.bUseInstanceCursor ? shadowPassFirstInstance[i] : 0U;
            shadowInputs.mCascadePerFrameBuffers[i] = mShadowPerFrameBuffers[i].Get();
            shadowInputs.mCascadePerFrameGroups[i]  = mShadowPerFrameGroups[i].Get();
        }

        Shadowing::AddDeferredCsmPassSet(shadowInputs, mGraphOutputs.mShadowMap);
    }

    void FBasicDeferredRenderer::RegisterDeferredSsaoPass(RenderCore::FFrameGraph& graph) {
        const auto* view         = mViewContext.View;
        auto*       outputTarget = mViewContext.OutputTarget;
        if (view == nullptr || outputTarget == nullptr) {
            Assert(false, TEXT("BasicDeferredRenderer"),
                "Render skipped: view/output target is null (view={}, outputTarget={}).",
                static_cast<int>(view ? 1 : 0), static_cast<int>(outputTarget ? 1 : 0));
            return;
        }
        if (!mGraphOutputs.mGBufferB.IsValid() || !mGraphOutputs.mSceneDepth.IsValid()) {
            return;
        }

        auto*       device    = Rhi::RHIGetDevice();
        auto&       resources = GetSharedResources();
        const auto* lights    = mViewContext.Lights;
        if (device == nullptr) {
            return;
        }

        EnsureLayouts(*device, resources);
        const bool bHasSsaoPipeline =
            EnsureFullscreenPipelineFromKeys(*device, resources, resources.SsaoVSKey,
                resources.SsaoPSKey, resources.SsaoPipelineLayout, resources.SsaoPipeline,
                TEXT("Deferred SSAO"), TEXT("BasicDeferred.SsaoPipeline"), false);
        if (!bHasSsaoPipeline || !resources.SsaoLayout || !resources.OutputSampler) {
            DebugAssert(false, TEXT("BasicDeferredRenderer"),
                "DeferredSsao skipped: shared pipeline/layout/sampler missing.");
            return;
        }

        AmbientOcclusion::FDeferredSsaoPassSetInputs ssaoInputs{};
        ssaoInputs.mGraph               = &graph;
        ssaoInputs.mView                = view;
        ssaoInputs.mLights              = lights;
        ssaoInputs.mPipeline            = resources.SsaoPipeline.Get();
        ssaoInputs.mLayout              = resources.SsaoLayout.Get();
        ssaoInputs.mSampler             = resources.OutputSampler.Get();
        ssaoInputs.mBindings            = &resources.SsaoBindings;
        ssaoInputs.mPerFrameBuffer      = mPerFrameBuffer.Get();
        ssaoInputs.mSsaoConstantsBuffer = mSsaoConstantsBuffer.Get();
        ssaoInputs.mGBufferB            = mGraphOutputs.mGBufferB;
        ssaoInputs.mSceneDepth          = mGraphOutputs.mSceneDepth;
        ssaoInputs.mDebugShadingMode    = gDeferredLightingDebugShadingMode;
        AmbientOcclusion::AddDeferredSsaoPassSet(ssaoInputs, mGraphOutputs.mSsaoTexture);
    }

    void FBasicDeferredRenderer::RegisterDeferredLightingPass(RenderCore::FFrameGraph& graph) {
        const auto* view         = mViewContext.View;
        auto*       outputTarget = mViewContext.OutputTarget;
        if (view == nullptr || outputTarget == nullptr) {
            Assert(false, TEXT("BasicDeferredRenderer"),
                "Render skipped: view/output target is null (view={}, outputTarget={}).",
                static_cast<int>(view ? 1 : 0), static_cast<int>(outputTarget ? 1 : 0));
            return;
        }
        if (!mGraphOutputs.mGBufferA.IsValid() || !mGraphOutputs.mGBufferB.IsValid()
            || !mGraphOutputs.mGBufferC.IsValid() || !mGraphOutputs.mSceneDepth.IsValid()) {
            return;
        }

        auto*       device    = Rhi::RHIGetDevice();
        auto&       resources = GetSharedResources();
        const auto* lights    = mViewContext.Lights;
        if (device == nullptr) {
            return;
        }

        Deferred::FCsmBuildInputs csmInputs{};
        csmInputs.View                      = view;
        csmInputs.Lights                    = lights;
        const Deferred::FCsmBuildResult csm = Deferred::BuildCsm(csmInputs);

        FPerFrameConstants              perFrameConstants{};
        BuildDeferredPerFrameConstants(*view, lights, csm, perFrameConstants);
        const auto viewRect = view->ViewRect;
        const u32  width    = view->RenderTargetExtent.Width;
        const u32  height   = view->RenderTargetExtent.Height;

        EnsureLayouts(*device, resources);
        Deferred::FDeferredLightingPassInputs lightingInputs{};
        if (BuildDeferredLightingPassInputs(graph, device, resources, viewRect, perFrameConstants,
                mViewContext, mPerFrameBuffer.Get(), mIblConstantsBuffer.Get(),
                mGraphOutputs.mGBufferA, mGraphOutputs.mGBufferB, mGraphOutputs.mGBufferC,
                mGraphOutputs.mSceneDepth, mGraphOutputs.mSsaoTexture, mGraphOutputs.mShadowMap,
                width, height, lightingInputs)) {
            Deferred::AddDeferredLightingPass(lightingInputs, mGraphOutputs.mSceneColorHdr);
        } else {
            DebugAssert(false, TEXT("BasicDeferredRenderer"),
                "DeferredLighting skipped: shared pipeline/layout/sampler missing.");
        }
    }

    void FBasicDeferredRenderer::RegisterDeferredSkyPass(RenderCore::FFrameGraph& graph) {
        const auto* view         = mViewContext.View;
        auto*       outputTarget = mViewContext.OutputTarget;
        if (view == nullptr || outputTarget == nullptr) {
            Assert(false, TEXT("BasicDeferredRenderer"),
                "Render skipped: view/output target is null (view={}, outputTarget={}).",
                static_cast<int>(view ? 1 : 0), static_cast<int>(outputTarget ? 1 : 0));
            return;
        }

        auto* device = Rhi::RHIGetDevice();
        if (device != nullptr) {
            auto& resources = GetSharedResources();
            EnsureLayouts(*device, resources);
            const auto                                 viewRect = view->ViewRect;

            Deferred::FDeferredAtmosphereSkyPassInputs atmosphereInputs{};
            if (BuildDeferredAtmosphereSkyPassInputs(graph, device, resources, viewRect,
                    mViewContext, mPerFrameBuffer.Get(), mGraphOutputs.mSceneDepth,
                    mGraphOutputs.mSceneColorHdr, atmosphereInputs)) {
                Deferred::AddDeferredAtmosphereSkyPass(atmosphereInputs);
            } else {
                Deferred::FDeferredSkyBoxPassInputs skyboxInputs{};
                if (BuildDeferredSkyBoxPassInputs(graph, device, resources, viewRect, mViewContext,
                        mPerFrameBuffer.Get(), mGraphOutputs.mSceneDepth,
                        mGraphOutputs.mSceneColorHdr, skyboxInputs)) {
                    Deferred::AddDeferredSkyBoxPass(skyboxInputs);
                }
            }
        }
    }

    void FBasicDeferredRenderer::RegisterDeferredPostProcessPass(RenderCore::FFrameGraph& graph) {
        const auto* view         = mViewContext.View;
        auto*       outputTarget = mViewContext.OutputTarget;
        if (view == nullptr || outputTarget == nullptr) {
            Assert(false, TEXT("BasicDeferredRenderer"),
                "Render skipped: view/output target is null (view={}, outputTarget={}).",
                static_cast<int>(view ? 1 : 0), static_cast<int>(outputTarget ? 1 : 0));
            return;
        }

        const bool bBackbufferOutput =
            (mViewContext.OutputFinalState == Rhi::ERhiResourceState::Present);
        auto outputTexture = bBackbufferOutput
            ? graph.ImportTextureLegacy(outputTarget, mViewContext.OutputFinalState)
            : graph.ImportTexture(Rhi::FRhiTextureRef(outputTarget), mViewContext.OutputFinalState);

        // Post-process chain (stack + registry) -> Present.
        {
            FPostProcessStack pp = BuildDefaultPostProcessStack();

            FPostProcessIO    io{};
            io.SceneColor = mGraphOutputs.mSceneColorHdr;
            io.Depth      = mGraphOutputs.mSceneDepth;

            FPostProcessBuildContext buildCtx{};
            buildCtx.ViewKey              = mViewContext.ViewKey;
            buildCtx.BackBuffer           = outputTexture;
            buildCtx.BackBufferFormat     = outputTarget->GetDesc().mFormat;
            buildCtx.BackBufferFinalState = mViewContext.OutputFinalState;

            BuildPostProcess(graph, *view, pp, io, buildCtx);
        }

        LogInfoCat(kFrameTimingCategory,
            TEXT(
                "Deferred.Pipelines baseHits={} baseMisses={} shadowHits={} shadowMisses={} materialLayoutMisses={} pipelineLayoutMisses={}"),
            gBasePassPipelineStats.mBaseHits, gBasePassPipelineStats.mBaseMisses,
            gBasePassPipelineStats.mShadowHits, gBasePassPipelineStats.mShadowMisses,
            gBasePassPipelineStats.mMaterialLayoutMisses,
            gBasePassPipelineStats.mPipelineLayoutMisses);
    }

    void FBasicDeferredRenderer::FinalizeRendering() {}
} // namespace AltinaEngine::Rendering
