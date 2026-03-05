
#include "Rendering/BasicDeferredRenderer.h"

#include "Rendering/DrawListExecutor.h"
#include "Deferred/DeferredTypes.h"
#include "Deferred/DeferredScenePasses.h"
#include "Deferred/DeferredSsaoPass.h"
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
#include "Container/Deque.h"
#include "Container/SmartPtr.h"
#include "Container/Vector.h"
#include "Logging/Log.h"
#include "Platform/Generic/GenericPlatformDecl.h"
#include "Rhi/Command/RhiCmdContext.h"
#include "Rhi/RhiBindGroup.h"
#include "Rhi/RhiBuffer.h"
#include "Rhi/RhiDevice.h"
#include "Rhi/RhiInit.h"
#include "Rhi/RhiPipeline.h"
#include "Rhi/RhiPipelineLayout.h"
#include "Rhi/RhiTexture.h"

#include "Math/Common.h"
#include "Math/LinAlg/Common.h"
#include "Math/LinAlg/RenderingMath.h"
#include "Algorithm/Sort.h"
#include "Utility/Assert.h"

#include <limits>

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
        using Container::TDeque;
        using Container::THashMap;
        using Container::TVector;
        using RenderCore::EMaterialPass;

        // 0=PBR, 1=Lambert(debug). Written into the DeferredLighting cbuffer.
        u32 gDeferredLightingDebugShadingMode = 0U;
        using Deferred::FIblConstants;
        using Deferred::FPerDrawConstants;
        using Deferred::FPerFrameConstants;

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
            out.BatchCount = static_cast<u32>(list.Batches.Size());

            FVector3f minWS(std::numeric_limits<f32>::max());
            FVector3f maxWS(-std::numeric_limits<f32>::max());

            for (const auto& batch : list.Batches) {
                const auto* mesh = batch.Static.Mesh;
                if (mesh == nullptr) {
                    continue;
                }
                if (batch.Static.LodIndex >= mesh->Lods.Size()) {
                    continue;
                }

                const auto& lodBounds = mesh->Lods[batch.Static.LodIndex].Bounds;
                if (!lodBounds.IsValid()) {
                    continue;
                }

                for (const auto& inst : batch.Instances) {
                    FVector3f instMinWS(0.0f);
                    FVector3f instMaxWS(0.0f);
                    if (!TransformAabbToWorld(inst.World, lodBounds, instMinWS, instMaxWS)) {
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
            }

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

        [[nodiscard]] constexpr auto GetVulkanPerDrawAlignment() noexcept -> u32 {
            // Conservative default for dynamic UBO offsets.
            return 256U;
        }

        [[nodiscard]] constexpr auto GetVulkanPerDrawCapacity() noexcept -> u32 {
            // Covers base pass + 4 shadow cascades for typical scenes with headroom.
            return 16384U;
        }

        auto BuildMaterialBindGroupLayoutDesc(const RenderCore::FMaterialLayout& materialLayout,
            TVector<Rhi::FRhiBindGroupLayoutEntry>& outEntries) -> u64 {
            outEntries.Clear();

            if (materialLayout.PropertyBag.IsValid()) {
                Rhi::FRhiBindGroupLayoutEntry entry{};
                entry.mBinding          = materialLayout.PropertyBag.GetBinding();
                entry.mType             = Rhi::ERhiBindingType::ConstantBuffer;
                entry.mVisibility       = Rhi::ERhiShaderStageFlags::All;
                entry.mArrayCount       = 1U;
                entry.mHasDynamicOffset = false;
                outEntries.PushBack(entry);
            }

            const usize textureCount = materialLayout.TextureBindings.Size();
            for (usize i = 0U; i < textureCount; ++i) {
                Rhi::FRhiBindGroupLayoutEntry entry{};
                entry.mBinding          = materialLayout.TextureBindings[i];
                entry.mType             = Rhi::ERhiBindingType::SampledTexture;
                entry.mVisibility       = Rhi::ERhiShaderStageFlags::All;
                entry.mArrayCount       = 1U;
                entry.mHasDynamicOffset = false;
                outEntries.PushBack(entry);
            }

            const usize samplerCount = materialLayout.SamplerBindings.Size();
            for (usize i = 0U; i < samplerCount; ++i) {
                const u32 samplerBinding = materialLayout.SamplerBindings[i];
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

        auto EnsureOutputPipeline(Rhi::FRhiDevice& device, FDeferredSharedResources& resources)
            -> bool {
            if (resources.OutputPipeline) {
                return true;
            }

            if (!resources.OutputVSKey.IsValid() || !resources.OutputPSKey.IsValid()) {
                LogError(TEXT("Deferred output shaders are not configured."));
                return false;
            }

            auto outputVs = resources.Registry.FindShader(resources.OutputVSKey);
            auto outputPs = resources.Registry.FindShader(resources.OutputPSKey);
            if (!outputVs || !outputPs) {
                LogError(TEXT("Deferred output shaders are not registered."));
                return false;
            }

            Rhi::FRhiGraphicsPipelineDesc outputDesc{};
            outputDesc.mDebugName.Assign(TEXT("BasicDeferred.OutputPipeline"));
            outputDesc.mVertexShader   = outputVs.Get();
            outputDesc.mPixelShader    = outputPs.Get();
            outputDesc.mPipelineLayout = resources.OutputPipelineLayout.Get();
            outputDesc.mVertexLayout   = {};
            outputDesc.mRasterState    = {};
            outputDesc.mDepthState     = {};
            outputDesc.mBlendState     = {};
            // Full-screen triangle in VSComposite is CW in NDC; avoid culling it.
            outputDesc.mRasterState.mCullMode   = Rhi::ERhiRasterCullMode::None;
            outputDesc.mDepthState.mDepthEnable = false;
            outputDesc.mDepthState.mDepthWrite  = false;
            resources.OutputPipeline            = device.CreateGraphicsPipeline(outputDesc);

            return resources.OutputPipeline.Get() != nullptr;
        }

        auto EnsureLightingPipeline(Rhi::FRhiDevice& device, FDeferredSharedResources& resources)
            -> bool {
            if (resources.LightingPipeline) {
                return true;
            }

            if (!resources.LightingVSKey.IsValid() || !resources.LightingPSKey.IsValid()) {
                LogError(TEXT("Deferred lighting shaders are not configured."));
                return false;
            }

            auto vs = resources.Registry.FindShader(resources.LightingVSKey);
            auto ps = resources.Registry.FindShader(resources.LightingPSKey);
            if (!vs || !ps) {
                LogError(TEXT("Deferred lighting shaders are not registered."));
                return false;
            }

            if (!resources.LightingPipelineLayout) {
                LogError(TEXT("Deferred lighting pipeline layout is missing."));
                return false;
            }

            Rhi::FRhiGraphicsPipelineDesc desc{};
            desc.mDebugName.Assign(TEXT("BasicDeferred.DeferredLightingPipeline"));
            desc.mVertexShader   = vs.Get();
            desc.mPixelShader    = ps.Get();
            desc.mPipelineLayout = resources.LightingPipelineLayout.Get();
            desc.mVertexLayout   = {};
            desc.mRasterState    = {};
            desc.mDepthState     = {};
            desc.mBlendState     = {};
            // Full-screen triangle; avoid culling.
            desc.mRasterState.mCullMode   = Rhi::ERhiRasterCullMode::None;
            desc.mDepthState.mDepthEnable = false;
            desc.mDepthState.mDepthWrite  = false;
            resources.LightingPipeline    = device.CreateGraphicsPipeline(desc);

            return resources.LightingPipeline.Get() != nullptr;
        }

        auto EnsureSsaoPipeline(Rhi::FRhiDevice& device, FDeferredSharedResources& resources)
            -> bool {
            if (resources.SsaoPipeline) {
                return true;
            }

            if (!resources.SsaoVSKey.IsValid() || !resources.SsaoPSKey.IsValid()) {
                LogError(TEXT("Deferred SSAO shaders are not configured."));
                return false;
            }

            auto vs = resources.Registry.FindShader(resources.SsaoVSKey);
            auto ps = resources.Registry.FindShader(resources.SsaoPSKey);
            if (!vs || !ps) {
                LogError(TEXT("Deferred SSAO shaders are not registered."));
                return false;
            }

            if (!resources.SsaoPipelineLayout) {
                LogError(TEXT("Deferred SSAO pipeline layout is missing."));
                return false;
            }

            Rhi::FRhiGraphicsPipelineDesc desc{};
            desc.mDebugName.Assign(TEXT("BasicDeferred.SsaoPipeline"));
            desc.mVertexShader            = vs.Get();
            desc.mPixelShader             = ps.Get();
            desc.mPipelineLayout          = resources.SsaoPipelineLayout.Get();
            desc.mVertexLayout            = {};
            desc.mRasterState             = {};
            desc.mDepthState              = {};
            desc.mBlendState              = {};
            desc.mRasterState.mCullMode   = Rhi::ERhiRasterCullMode::None;
            desc.mDepthState.mDepthEnable = false;
            desc.mDepthState.mDepthWrite  = false;
            resources.SsaoPipeline        = device.CreateGraphicsPipeline(desc);

            return resources.SsaoPipeline.Get() != nullptr;
        }

        auto EnsureSkyBoxPipeline(Rhi::FRhiDevice& device, FDeferredSharedResources& resources)
            -> bool {
            if (resources.SkyBoxPipeline) {
                return true;
            }

            if (!resources.SkyBoxVSKey.IsValid() || !resources.SkyBoxPSKey.IsValid()) {
                // Skybox is optional; missing shader keys is not an error.
                return false;
            }

            auto vs = resources.Registry.FindShader(resources.SkyBoxVSKey);
            auto ps = resources.Registry.FindShader(resources.SkyBoxPSKey);
            if (!vs || !ps) {
                LogError(TEXT("Deferred skybox shaders are not registered."));
                return false;
            }

            if (!resources.SkyBoxPipelineLayout) {
                LogError(TEXT("Deferred skybox pipeline layout is missing."));
                return false;
            }

            Rhi::FRhiGraphicsPipelineDesc desc{};
            desc.mDebugName.Assign(TEXT("BasicDeferred.SkyBoxPipeline"));
            desc.mVertexShader   = vs.Get();
            desc.mPixelShader    = ps.Get();
            desc.mPipelineLayout = resources.SkyBoxPipelineLayout.Get();
            desc.mVertexLayout   = {};
            desc.mRasterState    = {};
            desc.mDepthState     = {};
            desc.mBlendState     = {};
            // Full-screen triangle; avoid culling.
            desc.mRasterState.mCullMode   = Rhi::ERhiRasterCullMode::None;
            desc.mDepthState.mDepthEnable = false;
            desc.mDepthState.mDepthWrite  = false;
            resources.SkyBoxPipeline      = device.CreateGraphicsPipeline(desc);

            return resources.SkyBoxPipeline.Get() != nullptr;
        }

        void EnsureLayouts(Rhi::FRhiDevice& device, FDeferredSharedResources& resources) {
            TVector<RenderCore::FShaderRegistry::FShaderKey> basePassShaderKeys;
            if (resources.DefaultPassDesc.Shaders.Vertex.IsValid()) {
                basePassShaderKeys.PushBack(resources.DefaultPassDesc.Shaders.Vertex);
            }
            if (resources.DefaultPassDesc.Shaders.Pixel.IsValid()) {
                basePassShaderKeys.PushBack(resources.DefaultPassDesc.Shaders.Pixel);
            }
            if (resources.DefaultShadowPassDesc.Shaders.Vertex.IsValid()) {
                basePassShaderKeys.PushBack(resources.DefaultShadowPassDesc.Shaders.Vertex);
            }
            if (resources.DefaultShadowPassDesc.Shaders.Pixel.IsValid()) {
                basePassShaderKeys.PushBack(resources.DefaultShadowPassDesc.Shaders.Pixel);
            }

            if (!resources.PerFrameLayout) {
                u32                       setIndex   = 0U;
                u32                       binding    = 0U;
                Rhi::ERhiShaderStageFlags visibility = Rhi::ERhiShaderStageFlags::All;
                bool found = RenderCore::ShaderBinding::ResolveConstantBufferBindingByName(
                    resources.Registry, basePassShaderKeys, TEXT("ViewConstants"), setIndex,
                    binding, visibility);
                if (!found) {
                    found = RenderCore::ShaderBinding::ResolveConstantBufferBindingByName(
                        resources.Registry, basePassShaderKeys, TEXT("DeferredView"), setIndex,
                        binding, visibility);
                }

                DebugAssert(found, TEXT("BasicDeferredRenderer"),
                    "Failed to resolve per-frame cbuffer binding from shader reflection.");
                if (found) {
                    Rhi::FRhiBindGroupLayoutDesc layoutDesc{};
                    layoutDesc.mSetIndex = setIndex;

                    Rhi::FRhiBindGroupLayoutEntry entry{};
                    entry.mBinding    = binding;
                    entry.mType       = Rhi::ERhiBindingType::ConstantBuffer;
                    entry.mVisibility = visibility;
                    layoutDesc.mEntries.PushBack(entry);
                    layoutDesc.mLayoutHash =
                        BuildLayoutHash(layoutDesc.mEntries, layoutDesc.mSetIndex);
                    resources.PerFrameLayout  = device.CreateBindGroupLayout(layoutDesc);
                    resources.PerFrameBinding = binding;
                }
            }

            if (!resources.PerDrawLayout) {
                u32                       setIndex   = 0U;
                u32                       binding    = 0U;
                Rhi::ERhiShaderStageFlags visibility = Rhi::ERhiShaderStageFlags::All;
                const bool found = RenderCore::ShaderBinding::ResolveConstantBufferBindingByName(
                    resources.Registry, basePassShaderKeys, TEXT("ObjectConstants"), setIndex,
                    binding, visibility);
                DebugAssert(found, TEXT("BasicDeferredRenderer"),
                    "Failed to resolve per-draw cbuffer binding from shader reflection.");
                if (found) {
                    Rhi::FRhiBindGroupLayoutDesc layoutDesc{};
                    layoutDesc.mSetIndex = setIndex;

                    Rhi::FRhiBindGroupLayoutEntry entry{};
                    entry.mBinding          = binding;
                    entry.mType             = Rhi::ERhiBindingType::ConstantBuffer;
                    entry.mVisibility       = visibility;
                    entry.mHasDynamicOffset = IsVulkanBackend();
                    layoutDesc.mEntries.PushBack(entry);
                    layoutDesc.mLayoutHash =
                        BuildLayoutHash(layoutDesc.mEntries, layoutDesc.mSetIndex);
                    resources.PerDrawLayout  = device.CreateBindGroupLayout(layoutDesc);
                    resources.PerDrawBinding = binding;
                }
            }

            if (!resources.OutputLayout) {
                Rhi::FRhiBindGroupLayoutDesc                     layoutDesc{};
                TVector<RenderCore::FShaderRegistry::FShaderKey> shaderKeys;
                if (resources.OutputVSKey.IsValid()) {
                    shaderKeys.PushBack(resources.OutputVSKey);
                }
                if (resources.OutputPSKey.IsValid()) {
                    shaderKeys.PushBack(resources.OutputPSKey);
                }
                if (!shaderKeys.IsEmpty()) {
                    const bool built = RenderCore::ShaderBinding::BuildBindGroupLayoutFromShaderSet(
                        resources.Registry, shaderKeys, 0U, layoutDesc);
                    DebugAssert(built, TEXT("BasicDeferredRenderer"),
                        "Failed to build output bind group layout from shader reflection.");
                    if (built) {
                        resources.OutputLayout = device.CreateBindGroupLayout(layoutDesc);
                    }
                }
            }

            if (!resources.OutputSampler) {
                Rhi::FRhiSamplerDesc samplerDesc{};
                resources.OutputSampler = Rhi::RHICreateSampler(samplerDesc);
            }

            if (!resources.OutputPipelineLayout) {
                Rhi::FRhiPipelineLayoutDesc layoutDesc{};
                if (resources.OutputLayout) {
                    layoutDesc.mBindGroupLayouts.PushBack(resources.OutputLayout.Get());
                }
                resources.OutputPipelineLayout = device.CreatePipelineLayout(layoutDesc);
            }

            if (!resources.LightingLayout) {
                Rhi::FRhiBindGroupLayoutDesc                     layoutDesc{};
                TVector<RenderCore::FShaderRegistry::FShaderKey> shaderKeys;
                if (resources.LightingVSKey.IsValid()) {
                    shaderKeys.PushBack(resources.LightingVSKey);
                }
                if (resources.LightingPSKey.IsValid()) {
                    shaderKeys.PushBack(resources.LightingPSKey);
                }
                const bool built = RenderCore::ShaderBinding::BuildBindGroupLayoutFromShaderSet(
                    resources.Registry, shaderKeys, 0U, layoutDesc);
                DebugAssert(built, TEXT("BasicDeferredRenderer"),
                    "Failed to build lighting bind group layout from shader reflection.");
                if (built) {
                    resources.LightingLayout = device.CreateBindGroupLayout(layoutDesc);
                    DebugAssert(RenderCore::ShaderBinding::BuildBindingLookupTable(
                                    resources.Registry, shaderKeys, 0U,
                                    resources.LightingLayout.Get(), resources.LightingBindings),
                        TEXT("BasicDeferredRenderer"),
                        "Failed to build lighting binding lookup table from shader reflection.");
                }
            }

            if (!resources.SsaoLayout) {
                Rhi::FRhiBindGroupLayoutDesc                     layoutDesc{};
                TVector<RenderCore::FShaderRegistry::FShaderKey> shaderKeys;
                if (resources.SsaoVSKey.IsValid()) {
                    shaderKeys.PushBack(resources.SsaoVSKey);
                }
                if (resources.SsaoPSKey.IsValid()) {
                    shaderKeys.PushBack(resources.SsaoPSKey);
                }
                const bool built = RenderCore::ShaderBinding::BuildBindGroupLayoutFromShaderSet(
                    resources.Registry, shaderKeys, 0U, layoutDesc);
                DebugAssert(built, TEXT("BasicDeferredRenderer"),
                    "Failed to build SSAO bind group layout from shader reflection.");
                if (built) {
                    resources.SsaoLayout = device.CreateBindGroupLayout(layoutDesc);
                    DebugAssert(
                        RenderCore::ShaderBinding::BuildBindingLookupTable(resources.Registry,
                            shaderKeys, 0U, resources.SsaoLayout.Get(), resources.SsaoBindings),
                        TEXT("BasicDeferredRenderer"),
                        "Failed to build SSAO binding lookup table from shader reflection.");
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
                Rhi::FRhiPipelineLayoutDesc layoutDesc{};
                if (resources.LightingLayout) {
                    layoutDesc.mBindGroupLayouts.PushBack(resources.LightingLayout.Get());
                }
                resources.LightingPipelineLayout = device.CreatePipelineLayout(layoutDesc);
            }

            if (!resources.SsaoPipelineLayout) {
                Rhi::FRhiPipelineLayoutDesc layoutDesc{};
                if (resources.SsaoLayout) {
                    layoutDesc.mBindGroupLayouts.PushBack(resources.SsaoLayout.Get());
                }
                resources.SsaoPipelineLayout = device.CreatePipelineLayout(layoutDesc);
            }

            if (!resources.SkyBoxLayout) {
                Rhi::FRhiBindGroupLayoutDesc                     layoutDesc{};
                TVector<RenderCore::FShaderRegistry::FShaderKey> shaderKeys;
                if (resources.SkyBoxVSKey.IsValid()) {
                    shaderKeys.PushBack(resources.SkyBoxVSKey);
                }
                if (resources.SkyBoxPSKey.IsValid()) {
                    shaderKeys.PushBack(resources.SkyBoxPSKey);
                }
                const bool built = RenderCore::ShaderBinding::BuildBindGroupLayoutFromShaderSet(
                    resources.Registry, shaderKeys, 0U, layoutDesc);
                DebugAssert(built, TEXT("BasicDeferredRenderer"),
                    "Failed to build skybox bind group layout from shader reflection.");
                if (built) {
                    resources.SkyBoxLayout = device.CreateBindGroupLayout(layoutDesc);
                    DebugAssert(
                        RenderCore::ShaderBinding::BuildBindingLookupTable(resources.Registry,
                            shaderKeys, 0U, resources.SkyBoxLayout.Get(), resources.SkyBoxBindings),
                        TEXT("BasicDeferredRenderer"),
                        "Failed to build skybox binding lookup table from shader reflection.");
                }
            }

            if (!resources.SkyBoxPipelineLayout) {
                Rhi::FRhiPipelineLayoutDesc layoutDesc{};
                if (resources.SkyBoxLayout) {
                    layoutDesc.mBindGroupLayouts.PushBack(resources.SkyBoxLayout.Get());
                }
                resources.SkyBoxPipelineLayout = device.CreatePipelineLayout(layoutDesc);
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
                if (resources.DefaultPassDesc.Shaders.Vertex.IsValid()) {
                    shaderKeys.PushBack(resources.DefaultPassDesc.Shaders.Vertex);
                }
                if (resources.DefaultPassDesc.Shaders.Pixel.IsValid()) {
                    shaderKeys.PushBack(resources.DefaultPassDesc.Shaders.Pixel);
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
                    shaderLabel.Append(shaderKeys[i].Name.ToView());
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
            const auto&                            passLayout = resolvedPass->Layout;

            TVector<Rhi::FRhiBindGroupLayoutEntry> layoutEntries;
            const u64                              materialLayoutHash =
                BuildMaterialBindGroupLayoutDesc(passLayout, layoutEntries);

            Rhi::FRhiBindGroupLayoutRef materialLayoutRef;
            if (const auto it = resources.MaterialLayouts.FindIt(materialLayoutHash);
                it != resources.MaterialLayouts.end()) {
                materialLayoutRef = it->second;
            } else {
                Rhi::FRhiBindGroupLayoutDesc layoutDesc{};
                layoutDesc.mSetIndex   = IsVulkanBackend() ? 2U : 0U;
                layoutDesc.mEntries    = layoutEntries;
                layoutDesc.mLayoutHash = BuildLayoutHash(layoutDesc.mEntries, layoutDesc.mSetIndex);
                materialLayoutRef      = data->Device->CreateBindGroupLayout(layoutDesc);
                if (!materialLayoutRef) {
                    return nullptr;
                }
                resources.MaterialLayouts[materialLayoutHash] = materialLayoutRef;
                LogInfo(TEXT("BasePass MaterialLayout entries={} hash={}"),
                    static_cast<u32>(layoutEntries.Size()), materialLayoutHash);
                for (const auto& entry : layoutEntries) {
                    LogInfo(TEXT("  MaterialLayout binding={} type={} vis={}"), entry.mBinding,
                        static_cast<u32>(entry.mType), static_cast<u32>(entry.mVisibility));
                }
            }

            Rhi::FRhiPipelineLayoutRef pipelineLayout;
            if (const auto it = resources.BasePipelineLayouts.FindIt(materialLayoutHash);
                it != resources.BasePipelineLayouts.end()) {
                pipelineLayout = it->second;
            } else {
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
                LogInfo(TEXT("BasePass PipelineLayout groups={} hash={}"),
                    static_cast<u32>(layoutDesc.mBindGroupLayouts.Size()), materialLayoutHash);
            }

            const u64 key =
                batch.BatchKey.PipelineKey ^ (materialLayoutHash * 0x9e3779b97f4a7c15ULL);
            if (const auto it = data->PipelineCache->FindIt(key);
                it != data->PipelineCache->end()) {
                return it->second.Get();
            }

            const auto         vs = data->Registry->FindShader(resolvedPass->Shaders.Vertex);
            Rhi::FRhiShaderRef ps{};
            if (resolvedPass->Shaders.Pixel.IsValid()) {
                ps = data->Registry->FindShader(resolvedPass->Shaders.Pixel);
            }
            if (!vs) {
                return nullptr;
            }

            Rhi::FRhiGraphicsPipelineDesc desc{};
            desc.mDebugName.Assign(TEXT("BasicDeferred.BasePassPipeline"));
            desc.mVertexShader   = vs.Get();
            desc.mPixelShader    = ps ? ps.Get() : nullptr;
            desc.mPipelineLayout = pipelineLayout.Get();
            desc.mVertexLayout   = data->VertexLayout;
            desc.mRasterState    = resolvedPass->State.Raster;
            desc.mDepthState     = resolvedPass->State.Depth;
            desc.mBlendState     = resolvedPass->State.Blend;

            auto pipeline = data->Device->CreateGraphicsPipeline(desc);
            if (!pipeline) {
                return nullptr;
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
            synthetic.BatchKey.PipelineKey           = 0x43534D534841444FULL; // "CSMSHADO"
            auto shadowDesc                          = *data->DefaultPassDesc;
            shadowDesc.Shaders.Pixel                 = {};
            auto&                 resources          = GetSharedResources();
            FBasePassPipelineData shadowData         = *data;
            shadowData.PipelineCache                 = &resources.ShadowPipelines;
            return ResolveBasePassPipeline(synthetic, &shadowDesc, &shadowData);
        }

        struct FBasePassBindingData {
            Rhi::FRhiBuffer*    PerDrawBuffer = nullptr;
            Rhi::FRhiBindGroup* PerDrawGroup  = nullptr;
            // D3D11 backend uses a single set index (0) and distinguishes resources by register.
            u32                 PerDrawSetIndex    = 0U;
            u32                 PerDrawStrideBytes = 0U;
            u32                 PerDrawCapacity    = 0U;
            u32*                PerDrawWriteIndex  = nullptr;
            u32                 PerDrawBaseOffset  = 0U;
            bool                bUseDynamicOffset  = false;
        };

        void BindPerDraw(
            Rhi::FRhiCmdContext& ctx, const RenderCore::Render::FDrawBatch& batch, void* userData) {
            auto* data = static_cast<FBasePassBindingData*>(userData);
            if (!data || data->PerDrawBuffer == nullptr || data->PerDrawGroup == nullptr) {
                return;
            }

            if (batch.Instances.IsEmpty()) {
                return;
            }

            FPerDrawConstants constants{};
            constants.World        = batch.Instances[0].World;
            constants.NormalMatrix = Core::Math::LinAlg::ComputeNormalMatrix(constants.World);
            if (data->bUseDynamicOffset) {
                DebugAssert(data->PerDrawWriteIndex != nullptr, TEXT("BasicDeferredRenderer"),
                    "PerDraw dynamic cbuffer state is invalid: write index ptr is null.");
                const u32 writeIndex = (*data->PerDrawWriteIndex)++;
                DebugAssert(writeIndex < data->PerDrawCapacity, TEXT("BasicDeferredRenderer"),
                    "PerDraw dynamic cbuffer overflow (index={}, capacity={}).", writeIndex,
                    data->PerDrawCapacity);
                const u32 dynamicOffset =
                    data->PerDrawBaseOffset + writeIndex * data->PerDrawStrideBytes;
                ctx.RHIUpdateDynamicBufferDiscard(
                    data->PerDrawBuffer, &constants, sizeof(constants), dynamicOffset);
                ctx.RHISetBindGroup(data->PerDrawSetIndex, data->PerDrawGroup, &dynamicOffset, 1U);
                return;
            }

            // Update through the command context so D3D11 deferred contexts record the update with
            // the correct ordering relative to draws.
            ctx.RHIUpdateDynamicBufferDiscard(
                data->PerDrawBuffer, &constants, sizeof(constants), 0ULL);
            ctx.RHISetBindGroup(data->PerDrawSetIndex, data->PerDrawGroup, nullptr, 0U);
        }

        struct FShadowCascadeExecuteContext {
            const RenderCore::Render::FDrawList* ShadowDrawList = nullptr;
            FDrawListBindings                    DrawBindings{};
            FBasePassPipelineData                ShadowPipelineData{};
            FBasePassBindingData                 BindingData{};
            Rhi::FRhiBuffer*                     CascadePerFrameBuffers[4] = {};
            Rhi::FRhiBindGroup*                  CascadePerFrameGroups[4]  = {};
            Rhi::FRhiBuffer*                     FallbackPerFrameBuffer    = nullptr;
            Rhi::FRhiBindGroup*                  FallbackPerFrameGroup     = nullptr;
        };
    } // namespace

    auto FBasicDeferredRenderer::GetDefaultMaterialTemplate()
        -> Core::Container::TShared<RenderCore::FMaterialTemplate> {
        auto& resources = GetSharedResources();
        EnsureDefaultTemplate(resources);
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
        EnsureDefaultTemplate(resources);
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

        resources.OutputVSKey   = {};
        resources.OutputPSKey   = {};
        resources.LightingVSKey = {};
        resources.LightingPSKey = {};
        resources.SsaoVSKey     = {};
        resources.SsaoPSKey     = {};
        resources.SkyBoxVSKey   = {};
        resources.SkyBoxPSKey   = {};

        resources.Registry.Clear();
    }

    void FBasicDeferredRenderer::SetDeferredLightingDebugLambert(bool bEnabled) noexcept {
        gDeferredLightingDebugShadingMode = bEnabled ? 1U : 0U;
    }

    void FBasicDeferredRenderer::PrepareForRendering(Rhi::FRhiDevice& device) {
        auto& resources = GetSharedResources();
        EnsureDefaultTemplate(resources);
        EnsureVertexLayout(resources);
        EnsureLayouts(device, resources);
        if (!EnsureLightingPipeline(device, resources)) {
            Assert(false, TEXT("BasicDeferredRenderer"),
                "PrepareForRendering failed: lighting pipeline is unavailable.");
            return;
        }
        if (!EnsureSsaoPipeline(device, resources)) {
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

        if (IsVulkanBackend()) {
            mPerDrawStrideBytes = Core::Math::AlignUp(
                static_cast<u32>(sizeof(FPerDrawConstants)), GetVulkanPerDrawAlignment());
            mPerDrawCapacity = GetVulkanPerDrawCapacity();
        } else {
            mPerDrawStrideBytes = static_cast<u32>(sizeof(FPerDrawConstants));
            mPerDrawCapacity    = 1U;
        }

        const u64 perDrawBufferSize = static_cast<u64>(mPerDrawStrideBytes)
            * static_cast<u64>(mPerDrawCapacity)
            * static_cast<u64>(IsVulkanBackend() ? FBasicDeferredRenderer::kPerDrawFrameRing : 1U);

        if (!mPerDrawBuffer || mPerDrawBuffer->GetDesc().mSizeBytes != perDrawBufferSize) {
            mPerDrawGroup.Reset();
            mPerDrawBuffer.Reset();

            Rhi::FRhiBufferDesc desc{};
            desc.mDebugName.Assign(TEXT("Deferred.PerDraw"));
            desc.mSizeBytes = perDrawBufferSize;
            desc.mUsage     = Rhi::ERhiResourceUsage::Dynamic;
            desc.mBindFlags = Rhi::ERhiBufferBindFlags::Constant;
            desc.mCpuAccess = Rhi::ERhiCpuAccess::Write;
            mPerDrawBuffer  = device.CreateBuffer(desc);
        }

        if (!mPerDrawGroup && mPerDrawBuffer && resources.PerDrawLayout) {
            RenderCore::ShaderBinding::FBindGroupBuilder builder(resources.PerDrawLayout.Get());
            DebugAssert(builder.AddBuffer(resources.PerDrawBinding, mPerDrawBuffer.Get(), 0ULL,
                            static_cast<u64>(sizeof(FPerDrawConstants))),
                TEXT("BasicDeferredRenderer"), "Failed to add per-draw binding (binding={}).",
                resources.PerDrawBinding);
            Rhi::FRhiBindGroupDesc groupDesc{};
            DebugAssert(builder.Build(groupDesc), TEXT("BasicDeferredRenderer"),
                "Failed to build per-draw bind group desc from layout.");
            mPerDrawGroup = device.CreateBindGroup(groupDesc);
        }
    }
    void FBasicDeferredRenderer::Render(RenderCore::FFrameGraph& graph) {
        LogInfo(TEXT("Deferred Render Enter {}"), 1);
        const auto* view         = mViewContext.View;
        auto*       outputTarget = mViewContext.OutputTarget;
        const auto* drawList     = mViewContext.DrawList;
        if (view == nullptr || outputTarget == nullptr) {
            Assert(false, TEXT("BasicDeferredRenderer"),
                "Render skipped: view/output target is null (view={}, outputTarget={}).",
                static_cast<int>(view ? 1 : 0), static_cast<int>(outputTarget ? 1 : 0));
            return;
        }

        if (!view->IsValid()) {
            Assert(false, TEXT("BasicDeferredRenderer"), "Render skipped: view is invalid.");
            return;
        }

        const u32 width  = view->RenderTargetExtent.Width;
        const u32 height = view->RenderTargetExtent.Height;
        if (width == 0U || height == 0U) {
            Assert(false, TEXT("BasicDeferredRenderer"),
                "Render skipped: render extent is invalid {}x{}.", static_cast<u32>(width),
                static_cast<u32>(height));
            return;
        }

        const bool bVulkanPerDrawDynamic = IsVulkanBackend();
        if (bVulkanPerDrawDynamic) {
            DebugAssert(mPerDrawStrideBytes >= static_cast<u32>(sizeof(FPerDrawConstants)),
                TEXT("BasicDeferredRenderer"), "PerDraw stride invalid (stride={}, expected>={}).",
                mPerDrawStrideBytes, static_cast<u32>(sizeof(FPerDrawConstants)));
            DebugAssert(mPerDrawCapacity > 0U, TEXT("BasicDeferredRenderer"),
                "PerDraw capacity invalid (capacity={}).", mPerDrawCapacity);
            mPerDrawFrameSlot = static_cast<u32>(view->FrameIndex % kPerDrawFrameRing);
        } else {
            mPerDrawFrameSlot = 0U;
        }
        mPerDrawWriteIndex = 0U;

        struct FImportedExternalTexture {
            Rhi::FRhiTexture*                 Texture = nullptr;
            RenderCore::FFrameGraphTextureRef Ref;
        };
        TVector<FImportedExternalTexture> importedExternalTextures;

        auto                              findOrImportExternalTextureRef =
            [&graph, &importedExternalTextures](
                Rhi::FRhiTexture* texture) -> RenderCore::FFrameGraphTextureRef {
            if (texture == nullptr) {
                return {};
            }
            for (const auto& imported : importedExternalTextures) {
                if (imported.Texture == texture) {
                    return imported.Ref;
                }
            }

            const auto ref = graph.ImportTexture(texture, Rhi::ERhiResourceState::Common);
            importedExternalTextures.PushBack({ texture, ref });
            return ref;
        };

        auto registerExternalTextureRead =
            [&findOrImportExternalTextureRef](RenderCore::FFrameGraphPassBuilder& builder,
                Rhi::FRhiTexture*                                                 texture,
                Rhi::ERhiResourceState state) -> RenderCore::FFrameGraphTextureRef {
            const auto ref = findOrImportExternalTextureRef(texture);
            if (!ref.IsValid()) {
                return {};
            }
            return builder.Read(ref, state);
        };

        auto registerMaterialTextureReads =
            [&registerExternalTextureRead](RenderCore::FFrameGraphPassBuilder& builder,
                const RenderCore::FMaterial* material, RenderCore::EMaterialPass pass) -> void {
            if (material == nullptr) {
                return;
            }

            const auto* layout = material->FindLayout(pass);
            if (layout == nullptr) {
                return;
            }

            const auto& parameters = material->GetParameters();
            for (const auto paramId : layout->TextureNameHashes) {
                const auto* textureParam = parameters.FindTextureParam(paramId);
                if (textureParam == nullptr || !textureParam->SRV) {
                    continue;
                }
                auto* texture = textureParam->SRV->GetTexture();
                if (texture == nullptr) {
                    continue;
                }
                registerExternalTextureRead(
                    builder, texture, Rhi::ERhiResourceState::ShaderResource);
            }
        };

        if (mPerFrameBuffer) {
            FPerFrameConstants constants{};
            constants.ViewProjection = view->Matrices.ViewProjJittered;
            UpdateConstantBuffer(mPerFrameBuffer.Get(), &constants, sizeof(constants));
        }

        {
            static bool sLoggedBoundsOnce = false;
            if (!sLoggedBoundsOnce) {
                sLoggedBoundsOnce = true;

                if (drawList != nullptr) {
                    const auto bounds = ComputeDrawListWorldBounds(*drawList);
                    if (bounds.bValid) {
                        LogInfo(
                            TEXT(
                                "Scene WorldBounds(BasePass): instances={} batches={} min=({}, {}, {}) max=({}, {}, {})"),
                            bounds.InstanceCount, bounds.BatchCount, bounds.MinWS[0],
                            bounds.MinWS[1], bounds.MinWS[2], bounds.MaxWS[0], bounds.MaxWS[1],
                            bounds.MaxWS[2]);
                    } else {
                        LogInfo(
                            TEXT("Scene WorldBounds(BasePass): <invalid> instances={} batches={}"),
                            bounds.InstanceCount, bounds.BatchCount);
                    }
                }

                if (mViewContext.ShadowDrawList != nullptr) {
                    const auto bounds = ComputeDrawListWorldBounds(*mViewContext.ShadowDrawList);
                    if (bounds.bValid) {
                        LogInfo(
                            TEXT(
                                "Scene WorldBounds(ShadowPass): instances={} batches={} min=({}, {}, {}) max=({}, {}, {})"),
                            bounds.InstanceCount, bounds.BatchCount, bounds.MinWS[0],
                            bounds.MinWS[1], bounds.MinWS[2], bounds.MaxWS[0], bounds.MaxWS[1],
                            bounds.MaxWS[2]);
                    } else {
                        LogInfo(
                            TEXT(
                                "Scene WorldBounds(ShadowPass): <invalid> instances={} batches={}"),
                            bounds.InstanceCount, bounds.BatchCount);
                    }
                }

                LogInfo(TEXT("View OriginWS=({}, {}, {}) Near={} Far={} FovY(rad)={} ReverseZ={}"),
                    view->ViewOrigin[0], view->ViewOrigin[1], view->ViewOrigin[2],
                    view->Camera.NearPlane, view->Camera.FarPlane, view->Camera.VerticalFovRadians,
                    view->bReverseZ ? 1 : 0);
            }
        }

        constexpr Rhi::FRhiClearColor     kAlbedoClear{ 0.12f, 0.12f, 0.12f, 1.0f };
        constexpr Rhi::FRhiClearColor     kNormalClear{ 0.5f, 0.5f, 1.0f, 1.0f };
        constexpr Rhi::FRhiClearColor     kEmissiveClear{ 0.0f, 0.0f, 0.0f, 1.0f };

        RenderCore::FFrameGraphTextureRef gbufferA;
        RenderCore::FFrameGraphTextureRef gbufferB;
        RenderCore::FFrameGraphTextureRef gbufferC;
        RenderCore::FFrameGraphTextureRef sceneDepth;

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

        auto&             resources = GetSharedResources();

        FDrawListBindings drawBindings{};
        drawBindings.PerFrame = mPerFrameGroup.Get();
        drawBindings.PerFrameSetIndex =
            resources.PerFrameLayout ? resources.PerFrameLayout->GetDesc().mSetIndex : 0U;
        drawBindings.PerDrawSetIndex =
            resources.PerDrawLayout ? resources.PerDrawLayout->GetDesc().mSetIndex : 0U;
        drawBindings.PerMaterialSetIndex  = IsVulkanBackend() ? 2U : 0U;
        drawBindings.ResolvedVertexLayout = &resources.BaseVertexLayout;

        FBasePassPipelineData pipelineData{};
        pipelineData.Device          = Rhi::RHIGetDevice();
        pipelineData.Registry        = &resources.Registry;
        pipelineData.PipelineCache   = &resources.BasePipelines;
        pipelineData.DefaultPassDesc = &resources.DefaultPassDesc;
        pipelineData.VertexLayout    = resources.BaseVertexLayout;

        FBasePassBindingData bindingData{};
        bindingData.PerDrawBuffer      = mPerDrawBuffer.Get();
        bindingData.PerDrawGroup       = mPerDrawGroup.Get();
        bindingData.PerDrawSetIndex    = drawBindings.PerDrawSetIndex;
        bindingData.PerDrawStrideBytes = mPerDrawStrideBytes;
        bindingData.PerDrawCapacity    = mPerDrawCapacity;
        bindingData.PerDrawWriteIndex  = &mPerDrawWriteIndex;
        bindingData.PerDrawBaseOffset  = mPerDrawFrameSlot * mPerDrawCapacity * mPerDrawStrideBytes;
        bindingData.bUseDynamicOffset  = bVulkanPerDrawDynamic;
        if (bindingData.bUseDynamicOffset && bindingData.PerDrawBuffer != nullptr) {
            const u64 requiredBytes = static_cast<u64>(bindingData.PerDrawBaseOffset)
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
                    for (const auto& batch : drawList->Batches) {
                        registerMaterialTextureReads(builder, batch.Material, batch.Pass);
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

                gbufferA   = data.GBufferA;
                gbufferB   = data.GBufferB;
                gbufferC   = data.GBufferC;
                sceneDepth = data.Depth;
            },
            [drawList, drawBindings, pipelineData, bindingData, viewRect](Rhi::FRhiCmdContext& ctx,
                const RenderCore::FFrameGraphPassResources&, const FBasePassData&) -> void {
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
        // Shadow (Directional CSM).
        const auto*               lights         = mViewContext.Lights;
        const auto*               shadowDrawList = mViewContext.ShadowDrawList;

        Deferred::FCsmBuildInputs csmInputs{};
        csmInputs.View                        = view;
        csmInputs.Lights                      = lights;
        csmInputs.ShadowDrawList              = shadowDrawList;
        const Deferred::FCsmBuildResult   csm = Deferred::BuildCsm(csmInputs);

        RenderCore::FFrameGraphTextureRef shadowMap;
        {
            // FrameGraph executes after this function scope; keep one stable context per Render()
            // call in the current frame to avoid dangling/overwritten user-data pointers.
            static thread_local TDeque<FShadowCascadeExecuteContext> sShadowContexts;
            sShadowContexts.PushBack(FShadowCascadeExecuteContext{});
            auto& executeCtx                              = sShadowContexts.Back();
            executeCtx.ShadowDrawList                     = shadowDrawList;
            executeCtx.DrawBindings                       = drawBindings;
            executeCtx.ShadowPipelineData                 = pipelineData;
            executeCtx.ShadowPipelineData.DefaultPassDesc = &resources.DefaultShadowPassDesc;
            executeCtx.BindingData                        = bindingData;
            executeCtx.FallbackPerFrameBuffer             = mPerFrameBuffer.Get();
            executeCtx.FallbackPerFrameGroup              = mPerFrameGroup.Get();
            for (u32 i = 0U; i < 4U; ++i) {
                executeCtx.CascadePerFrameBuffers[i] = mShadowPerFrameBuffers[i].Get();
                executeCtx.CascadePerFrameGroups[i]  = mShadowPerFrameGroups[i].Get();
            }

            Deferred::FCsmShadowPassInputs shadowInputs{};
            shadowInputs.Graph                     = &graph;
            shadowInputs.View                      = view;
            shadowInputs.ShadowDrawList            = shadowDrawList;
            shadowInputs.Csm                       = &csm;
            shadowInputs.PersistentShadowMap       = &resources.ShadowMapCSM;
            shadowInputs.PersistentShadowMapSize   = &resources.ShadowMapCSMSize;
            shadowInputs.PersistentShadowMapLayers = &resources.ShadowMapCSMLayers;
            shadowInputs.ExecuteCascadeFn          = [](Rhi::FRhiCmdContext& ctx, u32 cascadeIndex,
                                                const FMatrix4x4f& lightViewProj, u32 shadowMapSize,
                                                void* userData) -> void {
                auto* executeData = static_cast<FShadowCascadeExecuteContext*>(userData);
                if (executeData == nullptr) {
                    return;
                }

                Rhi::FRhiBuffer*    perFrameBuffer = (cascadeIndex < 4U)
                                ? executeData->CascadePerFrameBuffers[cascadeIndex]
                                : executeData->FallbackPerFrameBuffer;
                Rhi::FRhiBindGroup* perFrameGroup  = (cascadeIndex < 4U)
                              ? executeData->CascadePerFrameGroups[cascadeIndex]
                              : executeData->FallbackPerFrameGroup;
                if (reinterpret_cast<usize>(perFrameBuffer) == static_cast<usize>(~0ULL)
                    || reinterpret_cast<usize>(perFrameGroup) == static_cast<usize>(~0ULL)) {
                    DebugAssert(false, TEXT("BasicDeferredRenderer"),
                                 "Shadow cascade execute has poisoned pointers (cascade={}, perFrameBuffer={}, perFrameGroup={}).",
                                 cascadeIndex, reinterpret_cast<usize>(perFrameBuffer),
                                 reinterpret_cast<usize>(perFrameGroup));
                    return;
                }

                if (perFrameBuffer != nullptr) {
                    FPerFrameConstants constants{};
                    constants.ViewProjection = lightViewProj;
                    ctx.RHIUpdateDynamicBufferDiscard(
                        perFrameBuffer, &constants, sizeof(constants), 0ULL);
                }

                Rhi::FRhiViewportRect viewport{};
                viewport.mX        = 0.0f;
                viewport.mY        = 0.0f;
                viewport.mWidth    = static_cast<f32>(shadowMapSize);
                viewport.mHeight   = static_cast<f32>(shadowMapSize);
                viewport.mMinDepth = 0.0f;
                viewport.mMaxDepth = 1.0f;
                ctx.RHISetViewport(viewport);

                Rhi::FRhiScissorRect scissor{};
                scissor.mX      = 0;
                scissor.mY      = 0;
                scissor.mWidth  = shadowMapSize;
                scissor.mHeight = shadowMapSize;
                ctx.RHISetScissor(scissor);

                if (executeData->ShadowDrawList != nullptr) {
                    auto bindings     = executeData->DrawBindings;
                    bindings.PerFrame = perFrameGroup;
                    FDrawListExecutor::ExecuteBasePass(ctx, *executeData->ShadowDrawList, bindings,
                                 ResolveShadowPassPipeline, &executeData->ShadowPipelineData, BindPerDraw,
                                 &executeData->BindingData);
                }
            };
            shadowInputs.ExecuteCascadeUserData = &executeCtx;

            Deferred::AddCsmShadowPasses(shadowInputs, shadowMap);
        }

        // Fill shared per-frame constants once (used by SSAO and deferred lighting).
        FPerFrameConstants perFrameConstants{};
        {
            perFrameConstants.ViewProjection = view->Matrices.ViewProjJittered;
            perFrameConstants.View           = view->Matrices.View;
            perFrameConstants.Proj           = view->Matrices.ProjJittered;
            perFrameConstants.ViewProj       = view->Matrices.ViewProjJittered;
            perFrameConstants.InvViewProj    = view->Matrices.InvViewProjJittered;

            perFrameConstants.ViewOriginWS[0]  = view->ViewOrigin[0];
            perFrameConstants.ViewOriginWS[1]  = view->ViewOrigin[1];
            perFrameConstants.ViewOriginWS[2]  = view->ViewOrigin[2];
            perFrameConstants.bReverseZ        = view->bReverseZ ? 1U : 0U;
            perFrameConstants.DebugShadingMode = gDeferredLightingDebugShadingMode;

            const f32 w = static_cast<f32>(view->RenderTargetExtent.Width);
            const f32 h = static_cast<f32>(view->RenderTargetExtent.Height);
            perFrameConstants.RenderTargetSize[0]    = w;
            perFrameConstants.RenderTargetSize[1]    = h;
            perFrameConstants.InvRenderTargetSize[0] = (w > 0.0f) ? (1.0f / w) : 0.0f;
            perFrameConstants.InvRenderTargetSize[1] = (h > 0.0f) ? (1.0f / h) : 0.0f;

            // Lighting inputs.
            RenderCore::Lighting::FDirectionalLight dir{};
            if (lights != nullptr && lights->bHasMainDirectionalLight) {
                dir = lights->MainDirectionalLight;
            } else {
                dir.DirectionWS  = FVector3f(0.4f, 0.6f, 0.7f);
                dir.Color        = FVector3f(1.0f, 1.0f, 1.0f);
                dir.Intensity    = 2.0f;
                dir.bCastShadows = false;
            }

            perFrameConstants.DirLightDirectionWS[0] = dir.DirectionWS[0];
            perFrameConstants.DirLightDirectionWS[1] = dir.DirectionWS[1];
            perFrameConstants.DirLightDirectionWS[2] = dir.DirectionWS[2];
            perFrameConstants.DirLightColor[0]       = dir.Color[0];
            perFrameConstants.DirLightColor[1]       = dir.Color[1];
            perFrameConstants.DirLightColor[2]       = dir.Color[2];
            perFrameConstants.DirLightIntensity      = dir.Intensity;

            // Point lights.
            perFrameConstants.PointLightCount = 0U;
            if (lights != nullptr && !lights->PointLights.IsEmpty()) {
                const u32 count = static_cast<u32>(lights->PointLights.Size());
                const u32 clamped =
                    (count > Deferred::kMaxPointLights) ? Deferred::kMaxPointLights : count;
                perFrameConstants.PointLightCount = clamped;
                for (u32 i = 0U; i < clamped; ++i) {
                    const auto& src                                = lights->PointLights[i];
                    perFrameConstants.PointLights[i].PositionWS[0] = src.PositionWS[0];
                    perFrameConstants.PointLights[i].PositionWS[1] = src.PositionWS[1];
                    perFrameConstants.PointLights[i].PositionWS[2] = src.PositionWS[2];
                    perFrameConstants.PointLights[i].Range         = src.Range;
                    perFrameConstants.PointLights[i].Color[0]      = src.Color[0];
                    perFrameConstants.PointLights[i].Color[1]      = src.Color[1];
                    perFrameConstants.PointLights[i].Color[2]      = src.Color[2];
                    perFrameConstants.PointLights[i].Intensity     = src.Intensity;
                }
            }

            Deferred::FillPerFrameCsmConstants(csm, perFrameConstants);
        }

        RenderCore::FFrameGraphTextureRef ssaoTexture;

        {
            auto& shared = GetSharedResources();
            auto* device = Rhi::RHIGetDevice();
            if (device != nullptr && EnsureSsaoPipeline(*device, shared) && shared.SsaoLayout
                && shared.SsaoPipeline && shared.OutputSampler) {
                Deferred::FDeferredSsaoPassInputs ssaoInputs{};
                ssaoInputs.Graph                  = &graph;
                ssaoInputs.ViewRect               = &viewRect;
                ssaoInputs.PerFrameConstants      = &perFrameConstants;
                ssaoInputs.Pipeline               = shared.SsaoPipeline.Get();
                ssaoInputs.Layout                 = shared.SsaoLayout.Get();
                ssaoInputs.Sampler                = shared.OutputSampler.Get();
                ssaoInputs.Bindings               = &shared.SsaoBindings;
                ssaoInputs.PerFrameBuffer         = mPerFrameBuffer.Get();
                ssaoInputs.SsaoConstantsBuffer    = mSsaoConstantsBuffer.Get();
                ssaoInputs.GBufferB               = gbufferB;
                ssaoInputs.SceneDepth             = sceneDepth;
                ssaoInputs.Width                  = width;
                ssaoInputs.Height                 = height;
                ssaoInputs.RuntimeSettings.Enable = (rSsaoEnable.GetRenderValue() != 0) ? 1U : 0U;
                ssaoInputs.RuntimeSettings.SampleCount =
                    static_cast<u32>(Core::Math::Max(0, rSsaoSampleCount.GetRenderValue()));
                ssaoInputs.RuntimeSettings.RadiusVS  = rSsaoRadiusVS.GetRenderValue();
                ssaoInputs.RuntimeSettings.BiasNdc   = rSsaoBiasNdc.GetRenderValue();
                ssaoInputs.RuntimeSettings.Power     = rSsaoPower.GetRenderValue();
                ssaoInputs.RuntimeSettings.Intensity = rSsaoIntensity.GetRenderValue();

                Deferred::AddDeferredSsaoPass(ssaoInputs, ssaoTexture);
            }
        }

        auto outputTexture = graph.ImportTexture(outputTarget, Rhi::ERhiResourceState::Present);
        RenderCore::FFrameGraphTextureRef sceneColorHDR;

        auto*                             device = Rhi::RHIGetDevice();
        if (device != nullptr) {
            auto& shared = GetSharedResources();
            EnsureLayouts(*device, shared);

            if (EnsureLightingPipeline(*device, shared) && shared.LightingLayout
                && shared.LightingPipeline && shared.OutputSampler) {
                Deferred::FDeferredLightingPassInputs lightingInputs{};
                lightingInputs.Graph                      = &graph;
                lightingInputs.ViewRect                   = &viewRect;
                lightingInputs.PerFrameConstants          = &perFrameConstants;
                lightingInputs.Pipeline                   = shared.LightingPipeline.Get();
                lightingInputs.Layout                     = shared.LightingLayout.Get();
                lightingInputs.Sampler                    = shared.OutputSampler.Get();
                lightingInputs.Bindings                   = &shared.LightingBindings;
                lightingInputs.PerFrameBuffer             = mPerFrameBuffer.Get();
                lightingInputs.IblConstantsBuffer         = mIblConstantsBuffer.Get();
                lightingInputs.GBufferA                   = gbufferA;
                lightingInputs.GBufferB                   = gbufferB;
                lightingInputs.GBufferC                   = gbufferC;
                lightingInputs.SceneDepth                 = sceneDepth;
                lightingInputs.SsaoTexture                = ssaoTexture;
                lightingInputs.ShadowMap                  = shadowMap;
                lightingInputs.Width                      = width;
                lightingInputs.Height                     = height;
                lightingInputs.SkyIrradiance              = mViewContext.SkyIrradianceCube;
                lightingInputs.SkySpecular                = mViewContext.SkySpecularCube;
                lightingInputs.BrdfLut                    = mViewContext.BrdfLutTexture;
                lightingInputs.IblBlackCube               = shared.IblBlackCube.Get();
                lightingInputs.IblBlack2D                 = shared.IblBlack2D.Get();
                lightingInputs.RuntimeSettings.bEnableIbl = (rIblEnable.GetRenderValue() != 0)
                    && mViewContext.bHasSkyIbl && (mViewContext.SkyIrradianceCube != nullptr)
                    && (mViewContext.SkySpecularCube != nullptr)
                    && (mViewContext.BrdfLutTexture != nullptr);
                lightingInputs.RuntimeSettings.IblDiffuseIntensity =
                    rIblDiffuseIntensity.GetRenderValue();
                lightingInputs.RuntimeSettings.IblSpecularIntensity =
                    rIblSpecularIntensity.GetRenderValue();
                lightingInputs.RuntimeSettings.IblSaturation  = rIblSaturation.GetRenderValue();
                lightingInputs.RuntimeSettings.SpecularMaxLod = mViewContext.SkySpecularMaxLod;

                Deferred::AddDeferredLightingPass(lightingInputs, sceneColorHDR);
            } else {
                DebugAssert(false, TEXT("BasicDeferredRenderer"),
                    "DeferredLighting skipped: shared pipeline/layout/sampler missing.");
            }

            if (mViewContext.bHasSkyCube && (mViewContext.SkyCubeTexture != nullptr)
                && sceneColorHDR.IsValid() && sceneDepth.IsValid() && shared.SkyBoxLayout
                && EnsureSkyBoxPipeline(*device, shared) && shared.SkyBoxPipeline
                && shared.OutputSampler) {
                Deferred::FDeferredSkyBoxPassInputs skyboxInputs{};
                skyboxInputs.Graph          = &graph;
                skyboxInputs.ViewRect       = &viewRect;
                skyboxInputs.Pipeline       = shared.SkyBoxPipeline.Get();
                skyboxInputs.Layout         = shared.SkyBoxLayout.Get();
                skyboxInputs.Sampler        = shared.OutputSampler.Get();
                skyboxInputs.Bindings       = &shared.SkyBoxBindings;
                skyboxInputs.PerFrameBuffer = mPerFrameBuffer.Get();
                skyboxInputs.SkyCube        = mViewContext.SkyCubeTexture;
                skyboxInputs.SceneDepth     = sceneDepth;
                skyboxInputs.SceneColorHDR  = sceneColorHDR;
                Deferred::AddDeferredSkyBoxPass(skyboxInputs);
            }
        }

        // Post-process chain (stack + registry) -> Present.
        {
            FPostProcessStack pp{};
            pp.bEnable = (rPostProcessEnable.GetRenderValue() != 0);

            const bool bEnableTaa = (rPostProcessTaa.GetRenderValue() != 0);
            if (bEnableTaa) {
                FPostProcessNode node{};
                node.EffectId.Assign(TEXT("TAA"));
                node.bEnabled = true;
                node.Params[FString(TEXT("Alpha"))] =
                    FPostProcessParamValue(rPostProcessTaaAlpha.GetRenderValue());
                node.Params[FString(TEXT("ClampK"))] =
                    FPostProcessParamValue(rPostProcessTaaClampK.GetRenderValue());
                pp.Stack.PushBack(Move(node));
            }

            if (rPostProcessBloom.GetRenderValue() != 0) {
                FPostProcessNode node{};
                node.EffectId.Assign(TEXT("Bloom"));
                node.bEnabled = true;
                // Defaults can be tuned via CVars; can be overridden later via stack params.
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
                pp.Stack.PushBack(Move(node));
            }

            if (rPostProcessTonemap.GetRenderValue() != 0) {
                FPostProcessNode node{};
                node.EffectId.Assign(TEXT("Tonemap"));
                node.bEnabled = true;
                // Defaults; user can override via stack params later.
                node.Params[FString(TEXT("Exposure"))] = FPostProcessParamValue(1.0f);
                node.Params[FString(TEXT("Gamma"))]    = FPostProcessParamValue(2.2f);
                pp.Stack.PushBack(Move(node));
            }

            if (!bEnableTaa && rPostProcessFxaa.GetRenderValue() != 0) {
                FPostProcessNode node{};
                node.EffectId.Assign(TEXT("Fxaa"));
                node.bEnabled = true;
                // Defaults can be tuned via CVars; can be overridden later via stack params.
                node.Params[FString(TEXT("EdgeThreshold"))] =
                    FPostProcessParamValue(rPostProcessFxaaEdgeThreshold.GetRenderValue());
                node.Params[FString(TEXT("EdgeThresholdMin"))] =
                    FPostProcessParamValue(rPostProcessFxaaEdgeThresholdMin.GetRenderValue());
                node.Params[FString(TEXT("Subpix"))] =
                    FPostProcessParamValue(rPostProcessFxaaSubpix.GetRenderValue());
                pp.Stack.PushBack(Move(node));
            }

            FPostProcessIO io{};
            io.SceneColor = sceneColorHDR;
            io.Depth      = sceneDepth;

            FPostProcessBuildContext buildCtx{};
            buildCtx.ViewKey          = mViewContext.ViewKey;
            buildCtx.BackBuffer       = outputTexture;
            buildCtx.BackBufferFormat = outputTarget->GetDesc().mFormat;

            BuildPostProcess(graph, *view, pp, io, buildCtx);
        }
    }

    void FBasicDeferredRenderer::FinalizeRendering() {}
} // namespace AltinaEngine::Rendering
