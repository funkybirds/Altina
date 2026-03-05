
#include "Rendering/BasicDeferredRenderer.h"

#include "Rendering/DrawListExecutor.h"

#include "FrameGraph/FrameGraph.h"
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

#include <algorithm>
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
        using Container::THashMap;
        using Container::TVector;
        using RenderCore::EMaterialPass;

        constexpr u32 kMaxPointLights = 16U;

        // 0=PBR, 1=Lambert(debug). Written into the DeferredLighting cbuffer.
        u32           gDeferredLightingDebugShadingMode = 0U;

        // Shared per-frame constants.
        //
        // IMPORTANT: BasePass (VSBase) and ShadowDepth (VSShadowDepth) only read ViewProjection,
        // so it must remain the first member.
        struct FPerFrameConstants {
            FMatrix4x4f ViewProjection;

            FMatrix4x4f View;
            FMatrix4x4f Proj;
            FMatrix4x4f ViewProj;
            FMatrix4x4f InvViewProj;

            f32         ViewOriginWS[3] = { 0.0f, 0.0f, 0.0f };
            u32         PointLightCount = 0U;

            f32         DirLightDirectionWS[3] = { 0.0f, -1.0f, 0.0f };
            f32         DirLightIntensity      = 0.0f;
            f32         DirLightColor[3]       = { 1.0f, 1.0f, 1.0f };
            u32         CSMCascadeCount        = 0U;

            // Keep these as explicit members (not an array). See DeferredLighting.hlsl:
            // Slang -> DXBC can mishandle row_major on matrix arrays, causing implicit transposes.
            FMatrix4x4f CSM_LightViewProj0{};
            FMatrix4x4f CSM_LightViewProj1{};
            FMatrix4x4f CSM_LightViewProj2{};
            FMatrix4x4f CSM_LightViewProj3{};
            f32         CSM_SplitsVS[RenderCore::Shadow::kMaxCascades][4] = {};

            f32         RenderTargetSize[2]    = { 0.0f, 0.0f };
            f32         InvRenderTargetSize[2] = { 0.0f, 0.0f };

            u32         bReverseZ           = 1U;
            u32         DebugShadingMode    = 0U; // 0=PBR, 1=Lambert(debug)
            f32         ShadowBias          = 0.0015f;
            f32         _pad0               = 0.0f;
            f32         ShadowMapInvSize[2] = { 0.0f, 0.0f };
            f32         _pad1[2]            = { 0.0f, 0.0f };

            struct FPointLight {
                f32 PositionWS[3] = { 0.0f, 0.0f, 0.0f };
                f32 Range         = 0.0f;
                f32 Color[3]      = { 1.0f, 1.0f, 1.0f };
                f32 Intensity     = 0.0f;
            };
            FPointLight PointLights[kMaxPointLights]{};
        };

        struct FPerDrawConstants {
            FMatrix4x4f World;
            FMatrix4x4f NormalMatrix;
        };

        // Matches `cbuffer IblConstants : register(b1)` in DeferredLighting.hlsl.
        struct FIblConstants {
            f32 EnvDiffuseIntensity  = 0.0f;
            f32 EnvSpecularIntensity = 0.0f;
            f32 SpecularMaxLod       = 0.0f;
            f32 EnvSaturation        = 1.0f;
        };

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

                    minWS[0] = std::min(minWS[0], instMinWS[0]);
                    minWS[1] = std::min(minWS[1], instMinWS[1]);
                    minWS[2] = std::min(minWS[2], instMinWS[2]);
                    maxWS[0] = std::max(maxWS[0], instMaxWS[0]);
                    maxWS[1] = std::max(maxWS[1], instMaxWS[1]);
                    maxWS[2] = std::max(maxWS[2], instMaxWS[2]);
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
            Rhi::FRhiPipelineLayoutRef                        LightingPipelineLayout;
            Rhi::FRhiPipelineRef                              LightingPipeline;

            // SSAO (FSQ -> AO texture).
            Rhi::FRhiBindGroupLayoutRef                       SsaoLayout;
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

        [[nodiscard]] auto MapSampledTextureBinding(u32 binding) noexcept -> u32 {
            return IsVulkanBackend() ? (1000U + binding) : binding;
        }

        [[nodiscard]] auto MapSamplerBinding(u32 binding) noexcept -> u32 {
            return IsVulkanBackend() ? (2000U + binding) : binding;
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

            Rhi::FRhiVertexAttributeDesc position{};
            position.mSemanticName.Assign(TEXT("POSITION"));
            position.mSemanticIndex     = 0U;
            position.mFormat            = Rhi::ERhiFormat::R32G32B32Float;
            position.mInputSlot         = 0U;
            position.mAlignedByteOffset = 0U;
            position.mPerInstance       = false;
            position.mInstanceStepRate  = 0U;

            resources.BaseVertexLayout.mAttributes.PushBack(position);

            Rhi::FRhiVertexAttributeDesc normal{};
            normal.mSemanticName.Assign(TEXT("NORMAL"));
            normal.mSemanticIndex     = 0U;
            normal.mFormat            = Rhi::ERhiFormat::R32G32B32Float;
            normal.mInputSlot         = 1U;
            normal.mAlignedByteOffset = 0U;
            normal.mPerInstance       = false;
            normal.mInstanceStepRate  = 0U;
            resources.BaseVertexLayout.mAttributes.PushBack(normal);

            Rhi::FRhiVertexAttributeDesc texcoord{};
            texcoord.mSemanticName.Assign(TEXT("TEXCOORD"));
            texcoord.mSemanticIndex     = 0U;
            texcoord.mFormat            = Rhi::ERhiFormat::R32G32Float;
            texcoord.mInputSlot         = 2U;
            texcoord.mAlignedByteOffset = 0U;
            texcoord.mPerInstance       = false;
            texcoord.mInstanceStepRate  = 0U;
            resources.BaseVertexLayout.mAttributes.PushBack(texcoord);
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
            if (const auto it = resources.MaterialLayouts.find(materialLayoutHash);
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
            if (const auto it = resources.BasePipelineLayouts.find(materialLayoutHash);
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
            if (const auto it = data->PipelineCache->find(key); it != data->PipelineCache->end()) {
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
        resources.MaterialLayouts.clear();
        resources.BasePipelineLayouts.clear();
        resources.BasePipelines.clear();
        resources.ShadowPipelines.clear();
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

        resources.SsaoPipeline.Reset();
        resources.SsaoPipelineLayout.Reset();
        resources.SsaoLayout.Reset();

        resources.SkyBoxPipeline.Reset();
        resources.SkyBoxPipelineLayout.Reset();
        resources.SkyBoxLayout.Reset();

        resources.IblBlackCube.Reset();
        resources.IblBlack2D.Reset();
        resources.ShadowMapCSM.Reset();
        resources.ShadowMapCSMSize   = 0U;
        resources.ShadowMapCSMLayers = 0U;

        resources.PerFrameLayout.Reset();
        resources.PerDrawLayout.Reset();
        resources.PerFrameBinding = 0U;
        resources.PerDrawBinding  = 0U;

        resources.BasePipelines.clear();
        resources.ShadowPipelines.clear();
        resources.MaterialLayouts.clear();
        resources.BasePipelineLayouts.clear();
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
            Rhi::FRhiBindGroupDesc groupDesc{};
            groupDesc.mLayout = resources.PerFrameLayout.Get();

            Rhi::FRhiBindGroupEntry entry{};
            entry.mBinding = resources.PerFrameBinding;
            entry.mType    = Rhi::ERhiBindingType::ConstantBuffer;
            entry.mBuffer  = mPerFrameBuffer.Get();
            entry.mOffset  = 0ULL;
            entry.mSize    = static_cast<u64>(sizeof(FPerFrameConstants));
            groupDesc.mEntries.PushBack(entry);

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
                    Rhi::FRhiBindGroupDesc groupDesc{};
                    groupDesc.mLayout = resources.PerFrameLayout.Get();

                    Rhi::FRhiBindGroupEntry entry{};
                    entry.mBinding = resources.PerFrameBinding;
                    entry.mType    = Rhi::ERhiBindingType::ConstantBuffer;
                    entry.mBuffer  = mShadowPerFrameBuffers[i].Get();
                    entry.mOffset  = 0ULL;
                    entry.mSize    = static_cast<u64>(sizeof(FPerFrameConstants));
                    groupDesc.mEntries.PushBack(entry);

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
            Rhi::FRhiBindGroupDesc groupDesc{};
            groupDesc.mLayout = resources.PerDrawLayout.Get();

            Rhi::FRhiBindGroupEntry entry{};
            entry.mBinding = resources.PerDrawBinding;
            entry.mType    = Rhi::ERhiBindingType::ConstantBuffer;
            entry.mBuffer  = mPerDrawBuffer.Get();
            entry.mOffset  = 0ULL;
            entry.mSize    = static_cast<u64>(sizeof(FPerDrawConstants));
            groupDesc.mEntries.PushBack(entry);

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
        drawBindings.PerMaterialSetIndex = IsVulkanBackend() ? 2U : 0U;

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
                // LogInfo(TEXT("FG Pass: BasicDeferred.BasePass"));
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
        const auto*                      lights         = mViewContext.Lights;
        const auto*                      shadowDrawList = mViewContext.ShadowDrawList;

        RenderCore::Shadow::FCSMSettings csmSettings{};
        {
            // Defaults: tuned for demos with large far planes. Keep the CSM range reasonable for
            // more stable results (less shimmering/aliasing).
            u32 cascades = RenderCore::Shadow::kMaxCascades;
            f32 lambda   = 0.65f;
            f32 maxDist  = 250.0f;
            u32 mapSize  = 2048U;
            f32 recvBias = 0.0015f;

            if (lights != nullptr && lights->bHasMainDirectionalLight) {
                const auto& dl = lights->MainDirectionalLight;
                cascades       = dl.ShadowCascadeCount;
                lambda         = dl.ShadowSplitLambda;
                maxDist        = dl.ShadowMaxDistance;
                mapSize        = dl.ShadowMapSize;
                recvBias       = dl.ShadowReceiverBias;
            }

            cascades = std::max(1U, std::min(cascades, RenderCore::Shadow::kMaxCascades));
            if (lambda < 0.0f) {
                lambda = 0.0f;
            } else if (lambda > 1.0f) {
                lambda = 1.0f;
            }
            if (maxDist < 0.0f) {
                maxDist = 0.0f;
            }
            if (mapSize == 0U) {
                mapSize = 2048U;
            }
            if (recvBias < 0.0f) {
                recvBias = 0.0f;
            }

            csmSettings.CascadeCount  = cascades;
            csmSettings.SplitLambda   = lambda;
            csmSettings.MaxDistance   = maxDist;
            csmSettings.ShadowMapSize = mapSize;
            csmSettings.ReceiverBias  = recvBias;
        }

        RenderCore::Shadow::FCSMData csmData{};
        if (lights != nullptr && lights->bHasMainDirectionalLight
            && lights->MainDirectionalLight.bCastShadows && shadowDrawList != nullptr) {
            RenderCore::Shadow::BuildDirectionalCSM(
                *view, lights->MainDirectionalLight, csmSettings, csmData);
        }

        // Debug: verify that the scene world AABB is actually inside each cascade's light
        // clip-space. This helps catch "shadow camera doesn't see the scene" issues quickly.
        {
            static bool sLoggedCsmCoverageOnce = false;
            if (!sLoggedCsmCoverageOnce && csmData.CascadeCount > 0U && shadowDrawList != nullptr) {
                sLoggedCsmCoverageOnce = true;

                const auto bounds = ComputeDrawListWorldBounds(*shadowDrawList);
                if (bounds.bValid) {
                    const FVector3f bmin = bounds.MinWS;
                    const FVector3f bmax = bounds.MaxWS;

                    FVector4f       cornersWS[8] = {
                        FVector4f(bmin[0], bmin[1], bmin[2], 1.0f),
                        FVector4f(bmax[0], bmin[1], bmin[2], 1.0f),
                        FVector4f(bmax[0], bmax[1], bmin[2], 1.0f),
                        FVector4f(bmin[0], bmax[1], bmin[2], 1.0f),
                        FVector4f(bmin[0], bmin[1], bmax[2], 1.0f),
                        FVector4f(bmax[0], bmin[1], bmax[2], 1.0f),
                        FVector4f(bmax[0], bmax[1], bmax[2], 1.0f),
                        FVector4f(bmin[0], bmax[1], bmax[2], 1.0f),
                    };

                    for (u32 cascade = 0U; cascade < csmData.CascadeCount; ++cascade) {
                        const auto& m = csmData.Cascades[cascade].LightViewProj;

                        f32         minNdcX = std::numeric_limits<f32>::max();
                        f32         minNdcY = std::numeric_limits<f32>::max();
                        f32         minNdcZ = std::numeric_limits<f32>::max();
                        f32         maxNdcX = -std::numeric_limits<f32>::max();
                        f32         maxNdcY = -std::numeric_limits<f32>::max();
                        f32         maxNdcZ = -std::numeric_limits<f32>::max();

                        for (u32 i = 0U; i < 8U; ++i) {
                            const auto clip = Core::Math::MatMul(m, cornersWS[i]);
                            const f32  invW = (std::abs(clip[3]) > 1e-6f) ? (1.0f / clip[3]) : 1.0f;
                            const f32  x    = clip[0] * invW;
                            const f32  y    = clip[1] * invW;
                            const f32  z    = clip[2] * invW;
                            minNdcX         = std::min(minNdcX, x);
                            minNdcY         = std::min(minNdcY, y);
                            minNdcZ         = std::min(minNdcZ, z);
                            maxNdcX         = std::max(maxNdcX, x);
                            maxNdcY         = std::max(maxNdcY, y);
                            maxNdcZ         = std::max(maxNdcZ, z);
                        }

                        // D3D NDC: x/y in [-1,1], z in [0,1].
                        LogInfo(
                            TEXT(
                                "CSM Coverage Cascade{}: sceneAabbNdc x=[{}, {}] y=[{}, {}] z=[{}, {}]"),
                            cascade, minNdcX, maxNdcX, minNdcY, maxNdcY, minNdcZ, maxNdcZ);
                    }
                } else {
                    LogInfo(TEXT("CSM Coverage: scene bounds invalid (shadowDrawList batches={})"),
                        bounds.BatchCount);
                }
            }
        }

        RenderCore::FFrameGraphTextureRef shadowMap;

        if (csmData.CascadeCount > 0U) {
            auto* device = Rhi::RHIGetDevice();
            Assert(device != nullptr, TEXT("BasicDeferredRenderer"),
                "Render failed: RHI device is null while preparing CSM shadow map.");

            const u32  shadowSize   = csmSettings.ShadowMapSize;
            const u32  shadowLayers = csmData.CascadeCount;

            const bool bNeedRecreateShadowMap = (!resources.ShadowMapCSM)
                || (resources.ShadowMapCSMSize != shadowSize)
                || (resources.ShadowMapCSMLayers != shadowLayers);
            if (bNeedRecreateShadowMap) {
                resources.ShadowMapCSM.Reset();

                Rhi::FRhiTextureDesc shadowDesc{};
                shadowDesc.mDebugName.Assign(TEXT("ShadowMap.CSM"));
                shadowDesc.mWidth       = shadowSize;
                shadowDesc.mHeight      = shadowSize;
                shadowDesc.mArrayLayers = shadowLayers;
                shadowDesc.mDimension = (shadowLayers > 1U) ? Rhi::ERhiTextureDimension::Tex2DArray
                                                            : Rhi::ERhiTextureDimension::Tex2D;
                shadowDesc.mFormat    = Rhi::ERhiFormat::D32Float;
                shadowDesc.mBindFlags = Rhi::ERhiTextureBindFlags::DepthStencil
                    | Rhi::ERhiTextureBindFlags::ShaderResource;

                resources.ShadowMapCSM = device->CreateTexture(shadowDesc);
                Assert(static_cast<bool>(resources.ShadowMapCSM), TEXT("BasicDeferredRenderer"),
                    "Render failed: CreateTexture(ShadowMap.CSM) returned null (size={}, layers={}).",
                    shadowSize, shadowLayers);

                resources.ShadowMapCSMSize   = shadowSize;
                resources.ShadowMapCSMLayers = shadowLayers;
            }

            shadowMap =
                graph.ImportTexture(resources.ShadowMapCSM.Get(), Rhi::ERhiResourceState::Common);
            Assert(shadowMap.IsValid(), TEXT("BasicDeferredRenderer"),
                "Render failed: ImportTexture(ShadowMap.CSM) returned invalid ref.");

            RenderCore::FFrameGraphPassDesc shadowPassDesc{};
            shadowPassDesc.mType  = RenderCore::EFrameGraphPassType::Raster;
            shadowPassDesc.mQueue = RenderCore::EFrameGraphQueue::Graphics;

            FBasePassPipelineData shadowPipelineData = pipelineData;
            shadowPipelineData.DefaultPassDesc       = &resources.DefaultShadowPassDesc;

            for (u32 cascade = 0U; cascade < csmData.CascadeCount; ++cascade) {
                struct FShadowPassData {
                    RenderCore::FFrameGraphTextureRef Shadow;
                    RenderCore::FFrameGraphDSVRef     ShadowDSV;
                };

                switch (cascade) {
                    case 0U:
                        shadowPassDesc.mName = "BasicDeferred.ShadowCSM.Cascade0";
                        break;
                    case 1U:
                        shadowPassDesc.mName = "BasicDeferred.ShadowCSM.Cascade1";
                        break;
                    case 2U:
                        shadowPassDesc.mName = "BasicDeferred.ShadowCSM.Cascade2";
                        break;
                    case 3U:
                        shadowPassDesc.mName = "BasicDeferred.ShadowCSM.Cascade3";
                        break;
                    default:
                        shadowPassDesc.mName = "BasicDeferred.ShadowCSM.Cascade";
                        break;
                }

                graph.AddPass<FShadowPassData>(
                    shadowPassDesc,
                    [&](RenderCore::FFrameGraphPassBuilder& builder, FShadowPassData& data) {
                        data.Shadow = builder.Write(shadowMap, Rhi::ERhiResourceState::DepthWrite);

                        if (shadowDrawList != nullptr) {
                            for (const auto& batch : shadowDrawList->Batches) {
                                registerMaterialTextureReads(builder, batch.Material, batch.Pass);
                            }
                        }

                        Rhi::FRhiTextureViewRange range{};
                        range.mBaseMip         = 0U;
                        range.mMipCount        = 1U;
                        range.mBaseArrayLayer  = cascade;
                        range.mLayerCount      = 1U;
                        range.mBaseDepthSlice  = 0U;
                        range.mDepthSliceCount = 1U;

                        Rhi::FRhiDepthStencilViewDesc dsvDesc{};
                        dsvDesc.mDebugName.Assign(TEXT("ShadowMap.CSM.DSV"));
                        dsvDesc.mFormat = Rhi::ERhiFormat::D32Float;
                        dsvDesc.mRange  = range;
                        data.ShadowDSV  = builder.CreateDSV(data.Shadow, dsvDesc);

                        RenderCore::FRdgDepthStencilBinding ds{};
                        ds.mDSV          = data.ShadowDSV;
                        ds.mDepthLoadOp  = Rhi::ERhiLoadOp::Clear;
                        ds.mDepthStoreOp = Rhi::ERhiStoreOp::Store;
                        ds.mClearDepthStencil.mDepth =
                            (view != nullptr && view->bReverseZ) ? 0.0f : 1.0f;
                        builder.SetRenderTargets(nullptr, 0U, &ds);
                    },
                    [shadowDrawList, drawBindings, shadowPipelineData, bindingData, shadowSize,
                        perFrameBuffer = (cascade < kShadowCascades)
                            ? mShadowPerFrameBuffers[cascade].Get()
                            : mPerFrameBuffer.Get(),
                        perFrameGroup  = (cascade < kShadowCascades)
                             ? mShadowPerFrameGroups[cascade].Get()
                             : mPerFrameGroup.Get(),
                        lightViewProj  = csmData.Cascades[cascade].LightViewProj](
                        Rhi::FRhiCmdContext& ctx, const RenderCore::FFrameGraphPassResources&,
                        const FShadowPassData&) -> void {
                        // LogInfo(TEXT("FG Pass: BasicDeferred.ShadowCSM"));

                        if (perFrameBuffer) {
                            FPerFrameConstants constants{};
                            constants.ViewProjection = lightViewProj;
                            ctx.RHIUpdateDynamicBufferDiscard(
                                perFrameBuffer, &constants, sizeof(constants), 0ULL);
                        }

                        Rhi::FRhiViewportRect viewport{};
                        viewport.mX        = 0.0f;
                        viewport.mY        = 0.0f;
                        viewport.mWidth    = static_cast<f32>(shadowSize);
                        viewport.mHeight   = static_cast<f32>(shadowSize);
                        viewport.mMinDepth = 0.0f;
                        viewport.mMaxDepth = 1.0f;
                        ctx.RHISetViewport(viewport);

                        Rhi::FRhiScissorRect scissor{};
                        scissor.mX      = 0;
                        scissor.mY      = 0;
                        scissor.mWidth  = shadowSize;
                        scissor.mHeight = shadowSize;
                        ctx.RHISetScissor(scissor);

                        if (shadowDrawList != nullptr) {
                            // Use the cascade-specific per-frame group so each pass reads the
                            // correct ViewProjection even on deferred contexts.
                            auto bindings     = drawBindings;
                            bindings.PerFrame = perFrameGroup;
                            FDrawListExecutor::ExecuteBasePass(ctx, *shadowDrawList, bindings,
                                ResolveShadowPassPipeline,
                                const_cast<FBasePassPipelineData*>(&shadowPipelineData),
                                BindPerDraw, const_cast<FBasePassBindingData*>(&bindingData));
                        }
                    });
            }
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
                const u32 count   = static_cast<u32>(lights->PointLights.Size());
                const u32 clamped = (count > kMaxPointLights) ? kMaxPointLights : count;
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

            // CSM.
            perFrameConstants.CSMCascadeCount = csmData.CascadeCount;
            perFrameConstants.ShadowBias      = csmSettings.ReceiverBias;
            if (csmSettings.ShadowMapSize > 0U) {
                const f32 inv = 1.0f / static_cast<f32>(csmSettings.ShadowMapSize);
                perFrameConstants.ShadowMapInvSize[0] = inv;
                perFrameConstants.ShadowMapInvSize[1] = inv;
            }

            for (u32 i = 0U; i < RenderCore::Shadow::kMaxCascades; ++i) {
                perFrameConstants.CSM_SplitsVS[i][0] = 0.0f;
                perFrameConstants.CSM_SplitsVS[i][1] = 0.0f;
                perFrameConstants.CSM_SplitsVS[i][2] = 0.0f;
                perFrameConstants.CSM_SplitsVS[i][3] = 0.0f;
            }

            if (csmData.CascadeCount > 0U) {
                for (u32 i = 0U; i < csmData.CascadeCount && i < RenderCore::Shadow::kMaxCascades;
                    ++i) {
                    perFrameConstants.CSM_SplitsVS[i][0] = csmData.Cascades[i].SplitVS[0];
                    perFrameConstants.CSM_SplitsVS[i][1] = csmData.Cascades[i].SplitVS[1];
                }

                // Copy up to 4 cascades into explicit matrices.
                perFrameConstants.CSM_LightViewProj0 = csmData.Cascades[0].LightViewProj;
                if (csmData.CascadeCount > 1U) {
                    perFrameConstants.CSM_LightViewProj1 = csmData.Cascades[1].LightViewProj;
                }
                if (csmData.CascadeCount > 2U) {
                    perFrameConstants.CSM_LightViewProj2 = csmData.Cascades[2].LightViewProj;
                }
                if (csmData.CascadeCount > 3U) {
                    perFrameConstants.CSM_LightViewProj3 = csmData.Cascades[3].LightViewProj;
                }
            }
        }

        RenderCore::FFrameGraphTextureRef ssaoTexture;

        // SSAO pass: produces an AO factor texture from GBuffer normal + depth (Reverse-Z aware).
        if (gbufferB.IsValid() && sceneDepth.IsValid()) {
            struct FSsaoPassData {
                RenderCore::FFrameGraphTextureRef Output;
                RenderCore::FFrameGraphRTVRef     OutputRTV;
                RenderCore::FFrameGraphTextureRef GBufferB;
                RenderCore::FFrameGraphTextureRef Depth;
            };

            RenderCore::FFrameGraphPassDesc ssaoPassDesc{};
            ssaoPassDesc.mName  = "BasicDeferred.SSAO";
            ssaoPassDesc.mType  = RenderCore::EFrameGraphPassType::Raster;
            ssaoPassDesc.mQueue = RenderCore::EFrameGraphQueue::Graphics;

            graph.AddPass<FSsaoPassData>(
                ssaoPassDesc,
                [&](RenderCore::FFrameGraphPassBuilder& builder, FSsaoPassData& data) -> void {
                    data.GBufferB = builder.Read(gbufferB, Rhi::ERhiResourceState::ShaderResource);
                    data.Depth = builder.Read(sceneDepth, Rhi::ERhiResourceState::ShaderResource);

                    RenderCore::FFrameGraphTextureDesc aoDesc{};
                    aoDesc.mDesc.mDebugName.Assign(TEXT("SSAO"));
                    aoDesc.mDesc.mWidth  = width;
                    aoDesc.mDesc.mHeight = height;
                    // R8Unorm is not available in current ERhiFormat list; use a single-channel
                    // float RT.
                    aoDesc.mDesc.mFormat    = Rhi::ERhiFormat::R32Float;
                    aoDesc.mDesc.mBindFlags = Rhi::ERhiTextureBindFlags::RenderTarget
                        | Rhi::ERhiTextureBindFlags::ShaderResource;

                    data.Output = builder.CreateTexture(aoDesc);
                    data.Output = builder.Write(data.Output, Rhi::ERhiResourceState::RenderTarget);

                    Rhi::FRhiRenderTargetViewDesc rtvDesc{};
                    rtvDesc.mDebugName.Assign(TEXT("SSAO.RTV"));
                    rtvDesc.mFormat = Rhi::ERhiFormat::R32Float;
                    data.OutputRTV  = builder.CreateRTV(data.Output, rtvDesc);

                    RenderCore::FRdgRenderTargetBinding rt{};
                    rt.mRTV        = data.OutputRTV;
                    rt.mLoadOp     = Rhi::ERhiLoadOp::Clear;
                    rt.mStoreOp    = Rhi::ERhiStoreOp::Store;
                    rt.mClearColor = Rhi::FRhiClearColor{ 1.0f, 1.0f, 1.0f, 1.0f };
                    builder.SetRenderTargets(&rt, 1U, nullptr);

                    ssaoTexture = data.Output;
                },
                [viewRect, sharedConstants = perFrameConstants,
                    perFrameBuffer = mPerFrameBuffer.Get(),
                    ssaoBuffer     = mSsaoConstantsBuffer.Get()](Rhi::FRhiCmdContext& ctx,
                    const RenderCore::FFrameGraphPassResources&                   res,
                    const FSsaoPassData&                                          data) -> void {
                    auto& shared = GetSharedResources();
                    if (!shared.SsaoPipeline || !shared.SsaoLayout || !shared.OutputSampler) {
                        return;
                    }

                    auto* normalTex = res.GetTexture(data.GBufferB);
                    auto* depthTex  = res.GetTexture(data.Depth);
                    auto* outTex    = res.GetTexture(data.Output);
                    if (!normalTex || !depthTex || !outTex) {
                        return;
                    }

                    auto* device = Rhi::RHIGetDevice();
                    if (!device) {
                        return;
                    }

                    if (perFrameBuffer == nullptr || ssaoBuffer == nullptr) {
                        return;
                    }

                    // Update per-frame constants (b0) so SSAO can reconstruct positions (Reverse-Z
                    // aware).
                    ctx.RHIUpdateDynamicBufferDiscard(
                        perFrameBuffer, &sharedConstants, sizeof(sharedConstants), 0ULL);

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

                    FSsaoConstants ssao{};
                    ssao.Enable = (rSsaoEnable.GetRenderValue() != 0) ? 1U : 0U;
                    ssao.SampleCount =
                        static_cast<u32>(std::max(0, rSsaoSampleCount.GetRenderValue()));
                    ssao.RadiusVS  = rSsaoRadiusVS.GetRenderValue();
                    ssao.BiasNdc   = rSsaoBiasNdc.GetRenderValue();
                    ssao.Power     = rSsaoPower.GetRenderValue();
                    ssao.Intensity = rSsaoIntensity.GetRenderValue();

                    ctx.RHIUpdateDynamicBufferDiscard(ssaoBuffer, &ssao, sizeof(ssao), 0ULL);

                    // Bind group (b0 + b1 + t0..t1 + s0).
                    Rhi::FRhiBindGroupDesc groupDesc{};
                    groupDesc.mLayout = shared.SsaoLayout.Get();

                    Rhi::FRhiBindGroupEntry cb{};
                    cb.mBinding = 0U;
                    cb.mType    = Rhi::ERhiBindingType::ConstantBuffer;
                    cb.mBuffer  = perFrameBuffer;
                    cb.mOffset  = 0ULL;
                    cb.mSize    = static_cast<u64>(sizeof(FPerFrameConstants));
                    groupDesc.mEntries.PushBack(cb);

                    Rhi::FRhiBindGroupEntry ssaoCb{};
                    ssaoCb.mBinding = 1U;
                    ssaoCb.mType    = Rhi::ERhiBindingType::ConstantBuffer;
                    ssaoCb.mBuffer  = ssaoBuffer;
                    ssaoCb.mOffset  = 0ULL;
                    ssaoCb.mSize    = static_cast<u64>(sizeof(FSsaoConstants));
                    groupDesc.mEntries.PushBack(ssaoCb);

                    Rhi::FRhiBindGroupEntry eNormal{};
                    eNormal.mBinding = MapSampledTextureBinding(0U);
                    eNormal.mType    = Rhi::ERhiBindingType::SampledTexture;
                    eNormal.mTexture = normalTex;
                    groupDesc.mEntries.PushBack(eNormal);

                    Rhi::FRhiBindGroupEntry eDepth{};
                    eDepth.mBinding = MapSampledTextureBinding(1U);
                    eDepth.mType    = Rhi::ERhiBindingType::SampledTexture;
                    eDepth.mTexture = depthTex;
                    groupDesc.mEntries.PushBack(eDepth);

                    Rhi::FRhiBindGroupEntry sampler{};
                    sampler.mBinding = MapSamplerBinding(0U);
                    sampler.mType    = Rhi::ERhiBindingType::Sampler;
                    sampler.mSampler = shared.OutputSampler.Get();
                    groupDesc.mEntries.PushBack(sampler);

                    auto bindGroup = device->CreateBindGroup(groupDesc);
                    Assert(!(!bindGroup), TEXT("BasicDeferredRenderer"),
                        "Failed to create bind group");

                    ctx.RHISetGraphicsPipeline(shared.SsaoPipeline.Get());

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

                    ctx.RHISetBindGroup(0U, bindGroup.Get(), nullptr, 0U);
                    ctx.RHISetPrimitiveTopology(Rhi::ERhiPrimitiveTopology::TriangleList);
                    ctx.RHIDraw(3U, 1U, 0U, 0U);
                });
        }

        auto outputTexture = graph.ImportTexture(outputTarget, Rhi::ERhiResourceState::Present);
        RenderCore::FFrameGraphTextureRef sceneColorHDR;

        struct FLightingPassData {
            RenderCore::FFrameGraphTextureRef Output;
            RenderCore::FFrameGraphRTVRef     OutputRTV;
            RenderCore::FFrameGraphTextureRef GBufferA;
            RenderCore::FFrameGraphTextureRef GBufferB;
            RenderCore::FFrameGraphTextureRef GBufferC;
            RenderCore::FFrameGraphTextureRef Depth;
            RenderCore::FFrameGraphTextureRef Shadow;
            RenderCore::FFrameGraphTextureRef Ssao;
        };

        RenderCore::FFrameGraphPassDesc lightingPassDesc{};
        lightingPassDesc.mName  = "BasicDeferred.DeferredLighting";
        lightingPassDesc.mType  = RenderCore::EFrameGraphPassType::Raster;
        lightingPassDesc.mQueue = RenderCore::EFrameGraphQueue::Graphics;

        graph.AddPass<FLightingPassData>(
            lightingPassDesc,
            [&](RenderCore::FFrameGraphPassBuilder& builder, FLightingPassData& data) {
                if (gbufferA.IsValid()) {
                    builder.Read(gbufferA, Rhi::ERhiResourceState::ShaderResource);
                }
                if (gbufferB.IsValid()) {
                    builder.Read(gbufferB, Rhi::ERhiResourceState::ShaderResource);
                }
                if (gbufferC.IsValid()) {
                    builder.Read(gbufferC, Rhi::ERhiResourceState::ShaderResource);
                }
                if (sceneDepth.IsValid()) {
                    builder.Read(sceneDepth, Rhi::ERhiResourceState::ShaderResource);
                }

                if (ssaoTexture.IsValid()) {
                    builder.Read(ssaoTexture, Rhi::ERhiResourceState::ShaderResource);
                }

                if (shadowMap.IsValid()) {
                    builder.Read(shadowMap, Rhi::ERhiResourceState::ShaderResource);
                    data.Shadow = shadowMap;
                } else {
                    // Dummy resource (not sampled when CSMCascadeCount == 0).
                    RenderCore::FFrameGraphTextureDesc shadowDesc{};
                    shadowDesc.mDesc.mDebugName.Assign(TEXT("ShadowMap.Dummy"));
                    shadowDesc.mDesc.mWidth       = 1U;
                    shadowDesc.mDesc.mHeight      = 1U;
                    shadowDesc.mDesc.mArrayLayers = 1U;
                    shadowDesc.mDesc.mFormat      = Rhi::ERhiFormat::D32Float;
                    shadowDesc.mDesc.mBindFlags   = Rhi::ERhiTextureBindFlags::DepthStencil
                        | Rhi::ERhiTextureBindFlags::ShaderResource;
                    shadowMap = builder.CreateTexture(shadowDesc);
                    builder.Read(shadowMap, Rhi::ERhiResourceState::ShaderResource);
                    data.Shadow = shadowMap;
                }

                if (mViewContext.SkyIrradianceCube != nullptr) {
                    registerExternalTextureRead(builder, mViewContext.SkyIrradianceCube,
                        Rhi::ERhiResourceState::ShaderResource);
                }
                if (mViewContext.SkySpecularCube != nullptr) {
                    registerExternalTextureRead(builder, mViewContext.SkySpecularCube,
                        Rhi::ERhiResourceState::ShaderResource);
                }
                if (mViewContext.BrdfLutTexture != nullptr) {
                    registerExternalTextureRead(builder, mViewContext.BrdfLutTexture,
                        Rhi::ERhiResourceState::ShaderResource);
                }
                if (resources.IblBlackCube) {
                    registerExternalTextureRead(builder, resources.IblBlackCube.Get(),
                        Rhi::ERhiResourceState::ShaderResource);
                }
                if (resources.IblBlack2D) {
                    registerExternalTextureRead(builder, resources.IblBlack2D.Get(),
                        Rhi::ERhiResourceState::ShaderResource);
                }

                data.GBufferA = gbufferA;
                data.GBufferB = gbufferB;
                data.GBufferC = gbufferC;
                data.Depth    = sceneDepth;
                data.Ssao     = ssaoTexture;

                // Lighting output goes to an internal HDR texture; post-process will write
                // backbuffer.
                RenderCore::FFrameGraphTextureDesc hdrDesc{};
                hdrDesc.mDesc.mDebugName.Assign(TEXT("SceneColorHDR"));
                hdrDesc.mDesc.mWidth       = width;
                hdrDesc.mDesc.mHeight      = height;
                hdrDesc.mDesc.mArrayLayers = 1U;
                hdrDesc.mDesc.mFormat      = Rhi::ERhiFormat::R16G16B16A16Float;
                hdrDesc.mDesc.mBindFlags   = Rhi::ERhiTextureBindFlags::RenderTarget
                    | Rhi::ERhiTextureBindFlags::ShaderResource;
                sceneColorHDR = builder.CreateTexture(hdrDesc);

                data.Output = builder.Write(sceneColorHDR, Rhi::ERhiResourceState::RenderTarget);

                Rhi::FRhiTextureViewRange viewRange{};
                viewRange.mMipCount        = 1U;
                viewRange.mLayerCount      = 1U;
                viewRange.mDepthSliceCount = 1U;

                Rhi::FRhiRenderTargetViewDesc rtvDesc{};
                rtvDesc.mDebugName.Assign(TEXT("SceneColorHDR.RTV"));
                rtvDesc.mFormat = hdrDesc.mDesc.mFormat;
                rtvDesc.mRange  = viewRange;
                data.OutputRTV  = builder.CreateRTV(data.Output, rtvDesc);

                RenderCore::FRdgRenderTargetBinding rtvBinding{};
                rtvBinding.mRTV        = data.OutputRTV;
                rtvBinding.mLoadOp     = Rhi::ERhiLoadOp::Clear;
                rtvBinding.mStoreOp    = Rhi::ERhiStoreOp::Store;
                rtvBinding.mClearColor = { 0.0f, 0.0f, 0.0f, 1.0f };

                builder.SetRenderTargets(&rtvBinding, 1U, nullptr);
            },
            [viewRect, sharedConstants = perFrameConstants, perFrameBuffer = mPerFrameBuffer.Get(),
                iblBuffer = mIblConstantsBuffer.Get(), bHasSkyIbl = mViewContext.bHasSkyIbl,
                skyIrradiance = mViewContext.SkyIrradianceCube,
                skySpecular = mViewContext.SkySpecularCube, brdfLut = mViewContext.BrdfLutTexture,
                specularMaxLod = mViewContext.SkySpecularMaxLod](Rhi::FRhiCmdContext& ctx,
                const RenderCore::FFrameGraphPassResources&                           res,
                const FLightingPassData& data) -> void {
                // LogInfo(TEXT("FG Pass: BasicDeferred.DeferredLighting"));
                auto& shared = GetSharedResources();
                if (!shared.LightingPipeline || !shared.LightingLayout || !shared.OutputSampler) {
                    DebugAssert(false, TEXT("BasicDeferredRenderer"),
                        "DeferredLighting skipped: shared pipeline/layout/sampler missing (pipeline={}, layout={}, sampler={}).",
                        static_cast<u32>(static_cast<bool>(shared.LightingPipeline)),
                        static_cast<u32>(static_cast<bool>(shared.LightingLayout)),
                        static_cast<u32>(static_cast<bool>(shared.OutputSampler)));
                    return;
                }

                auto* texA      = res.GetTexture(data.GBufferA);
                auto* texB      = res.GetTexture(data.GBufferB);
                auto* texC      = res.GetTexture(data.GBufferC);
                auto* depthTex  = res.GetTexture(data.Depth);
                auto* shadowTex = res.GetTexture(data.Shadow);
                auto* ssaoTex   = res.GetTexture(data.Ssao);
                if (!texA || !texB || !texC || !depthTex || !shadowTex || !ssaoTex) {
                    DebugAssert(false, TEXT("BasicDeferredRenderer"),
                        "DeferredLighting skipped: missing input textures (A={}, B={}, C={}, Depth={}, Shadow={}, Ssao={}).",
                        static_cast<u32>(texA != nullptr), static_cast<u32>(texB != nullptr),
                        static_cast<u32>(texC != nullptr), static_cast<u32>(depthTex != nullptr),
                        static_cast<u32>(shadowTex != nullptr),
                        static_cast<u32>(ssaoTex != nullptr));
                    return;
                }

                auto* device = Rhi::RHIGetDevice();
                if (!device) {
                    DebugAssert(false, TEXT("BasicDeferredRenderer"),
                        "DeferredLighting skipped: RHI device is null.");
                    return;
                }

                if (perFrameBuffer == nullptr) {
                    DebugAssert(false, TEXT("BasicDeferredRenderer"),
                        "DeferredLighting skipped: per-frame constant buffer is null.");
                    return;
                }

                // Fill per-frame constants (b0).
                ctx.RHIUpdateDynamicBufferDiscard(
                    perFrameBuffer, &sharedConstants, sizeof(sharedConstants), 0ULL);

                // Fill IBL constants (b1). If IBL is not available, keep intensities at zero.
                FIblConstants iblConstants{};
                const bool    bEnableIbl = (rIblEnable.GetRenderValue() != 0) && bHasSkyIbl
                    && (skyIrradiance != nullptr) && (skySpecular != nullptr)
                    && (brdfLut != nullptr);
                if (bEnableIbl) {
                    iblConstants.EnvDiffuseIntensity  = rIblDiffuseIntensity.GetRenderValue();
                    iblConstants.EnvSpecularIntensity = rIblSpecularIntensity.GetRenderValue();
                    iblConstants.SpecularMaxLod       = specularMaxLod;
                    iblConstants.EnvSaturation        = rIblSaturation.GetRenderValue();
                }
                if (iblBuffer != nullptr) {
                    ctx.RHIUpdateDynamicBufferDiscard(
                        iblBuffer, &iblConstants, sizeof(iblConstants), 0ULL);
                }

                // Bind group (b0 + b1 + t0..t8 + s0).
                Rhi::FRhiBindGroupDesc groupDesc{};
                groupDesc.mLayout = shared.LightingLayout.Get();

                Rhi::FRhiBindGroupEntry cb{};
                cb.mBinding = 0U;
                cb.mType    = Rhi::ERhiBindingType::ConstantBuffer;
                cb.mBuffer  = perFrameBuffer;
                cb.mOffset  = 0ULL;
                cb.mSize    = static_cast<u64>(sizeof(FPerFrameConstants));
                groupDesc.mEntries.PushBack(cb);

                Rhi::FRhiBindGroupEntry iblCb{};
                iblCb.mBinding = 1U;
                iblCb.mType    = Rhi::ERhiBindingType::ConstantBuffer;
                iblCb.mBuffer  = (iblBuffer != nullptr) ? iblBuffer : perFrameBuffer;
                iblCb.mOffset  = 0ULL;
                iblCb.mSize    = static_cast<u64>(sizeof(FIblConstants));
                groupDesc.mEntries.PushBack(iblCb);

                Rhi::FRhiBindGroupEntry eA{};
                eA.mBinding = MapSampledTextureBinding(0U);
                eA.mType    = Rhi::ERhiBindingType::SampledTexture;
                eA.mTexture = texA;
                groupDesc.mEntries.PushBack(eA);

                Rhi::FRhiBindGroupEntry eB{};
                eB.mBinding = MapSampledTextureBinding(1U);
                eB.mType    = Rhi::ERhiBindingType::SampledTexture;
                eB.mTexture = texB;
                groupDesc.mEntries.PushBack(eB);

                Rhi::FRhiBindGroupEntry eC{};
                eC.mBinding = MapSampledTextureBinding(2U);
                eC.mType    = Rhi::ERhiBindingType::SampledTexture;
                eC.mTexture = texC;
                groupDesc.mEntries.PushBack(eC);

                Rhi::FRhiBindGroupEntry eDepth{};
                eDepth.mBinding = MapSampledTextureBinding(3U);
                eDepth.mType    = Rhi::ERhiBindingType::SampledTexture;
                eDepth.mTexture = depthTex;
                groupDesc.mEntries.PushBack(eDepth);

                Rhi::FRhiBindGroupEntry eShadow{};
                eShadow.mBinding = MapSampledTextureBinding(4U);
                eShadow.mType    = Rhi::ERhiBindingType::SampledTexture;
                eShadow.mTexture = shadowTex;
                groupDesc.mEntries.PushBack(eShadow);

                // IBL textures (t5..t7).
                auto* irrTex  = bEnableIbl ? skyIrradiance : shared.IblBlackCube.Get();
                auto* specTex = bEnableIbl ? skySpecular : shared.IblBlackCube.Get();
                auto* lutTex  = bEnableIbl ? brdfLut : shared.IblBlack2D.Get();

                Rhi::FRhiBindGroupEntry eIrr{};
                eIrr.mBinding = MapSampledTextureBinding(5U);
                eIrr.mType    = Rhi::ERhiBindingType::SampledTexture;
                eIrr.mTexture = irrTex;
                groupDesc.mEntries.PushBack(eIrr);

                Rhi::FRhiBindGroupEntry eSpec{};
                eSpec.mBinding = MapSampledTextureBinding(6U);
                eSpec.mType    = Rhi::ERhiBindingType::SampledTexture;
                eSpec.mTexture = specTex;
                groupDesc.mEntries.PushBack(eSpec);

                Rhi::FRhiBindGroupEntry eLut{};
                eLut.mBinding = MapSampledTextureBinding(7U);
                eLut.mType    = Rhi::ERhiBindingType::SampledTexture;
                eLut.mTexture = lutTex;
                groupDesc.mEntries.PushBack(eLut);

                // SSAO (t8).
                Rhi::FRhiBindGroupEntry eSsao{};
                eSsao.mBinding = MapSampledTextureBinding(8U);
                eSsao.mType    = Rhi::ERhiBindingType::SampledTexture;
                eSsao.mTexture = ssaoTex;
                groupDesc.mEntries.PushBack(eSsao);

                Rhi::FRhiBindGroupEntry sampler{};
                sampler.mBinding = MapSamplerBinding(0U);
                sampler.mType    = Rhi::ERhiBindingType::Sampler;
                sampler.mSampler = shared.OutputSampler.Get();
                groupDesc.mEntries.PushBack(sampler);

                auto bindGroup = device->CreateBindGroup(groupDesc);
                Assert(!(!bindGroup), TEXT("BasicDeferredRenderer"), "Failed to create bind group");

                ctx.RHISetGraphicsPipeline(shared.LightingPipeline.Get());

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

                ctx.RHISetBindGroup(0U, bindGroup.Get(), nullptr, 0U);
                ctx.RHISetPrimitiveTopology(Rhi::ERhiPrimitiveTopology::TriangleList);
                ctx.RHIDraw(3U, 1U, 0U, 0U);
            });

        // Optional skybox pass: draw cube map into SceneColorHDR as background.
        if (mViewContext.bHasSkyCube && (mViewContext.SkyCubeTexture != nullptr)
            && sceneColorHDR.IsValid() && sceneDepth.IsValid()) {
            auto* device = Rhi::RHIGetDevice();
            if (device != nullptr) {
                auto& shared = GetSharedResources();
                EnsureLayouts(*device, shared);
                if (EnsureSkyBoxPipeline(*device, shared) && shared.SkyBoxLayout
                    && shared.SkyBoxPipeline) {
                    struct FSkyBoxPassData {
                        RenderCore::FFrameGraphTextureRef Depth;
                        RenderCore::FFrameGraphTextureRef Output;
                        RenderCore::FFrameGraphRTVRef     OutputRTV;
                    };

                    RenderCore::FFrameGraphPassDesc passDesc{};
                    passDesc.mName  = "BasicDeferred.SkyBox";
                    passDesc.mType  = RenderCore::EFrameGraphPassType::Raster;
                    passDesc.mQueue = RenderCore::EFrameGraphQueue::Graphics;

                    Rhi::FRhiTexture* skyCube = mViewContext.SkyCubeTexture;
                    graph.AddPass<FSkyBoxPassData>(
                        passDesc,
                        [&](RenderCore::FFrameGraphPassBuilder& builder, FSkyBoxPassData& data) {
                            builder.Read(sceneDepth, Rhi::ERhiResourceState::ShaderResource);
                            data.Depth = sceneDepth;
                            registerExternalTextureRead(
                                builder, skyCube, Rhi::ERhiResourceState::ShaderResource);

                            data.Output =
                                builder.Write(sceneColorHDR, Rhi::ERhiResourceState::RenderTarget);

                            Rhi::FRhiTextureViewRange viewRange{};
                            viewRange.mMipCount        = 1U;
                            viewRange.mLayerCount      = 1U;
                            viewRange.mDepthSliceCount = 1U;

                            Rhi::FRhiRenderTargetViewDesc rtvDesc{};
                            rtvDesc.mDebugName.Assign(TEXT("SceneColorHDR.SkyBox.RTV"));
                            rtvDesc.mFormat = Rhi::ERhiFormat::R16G16B16A16Float;
                            rtvDesc.mRange  = viewRange;
                            data.OutputRTV  = builder.CreateRTV(data.Output, rtvDesc);

                            RenderCore::FRdgRenderTargetBinding rtvBinding{};
                            rtvBinding.mRTV     = data.OutputRTV;
                            rtvBinding.mLoadOp  = Rhi::ERhiLoadOp::Load;
                            rtvBinding.mStoreOp = Rhi::ERhiStoreOp::Store;
                            builder.SetRenderTargets(&rtvBinding, 1U, nullptr);
                        },
                        [viewRect, perFrameBuffer = mPerFrameBuffer.Get(), skyCube](
                            Rhi::FRhiCmdContext&                        ctx,
                            const RenderCore::FFrameGraphPassResources& res,
                            const FSkyBoxPassData&                      data) -> void {
                            auto& shared = GetSharedResources();
                            if (!shared.SkyBoxPipeline || !shared.SkyBoxLayout
                                || !shared.OutputSampler || perFrameBuffer == nullptr
                                || skyCube == nullptr) {
                                DebugAssert(false, TEXT("BasicDeferredRenderer"),
                                    "SkyBox skipped: shared/resource missing (pipeline={}, layout={}, sampler={}, perFrameBuffer={}, skyCube={}).",
                                    static_cast<u32>(static_cast<bool>(shared.SkyBoxPipeline)),
                                    static_cast<u32>(static_cast<bool>(shared.SkyBoxLayout)),
                                    static_cast<u32>(static_cast<bool>(shared.OutputSampler)),
                                    static_cast<u32>(perFrameBuffer != nullptr),
                                    static_cast<u32>(skyCube != nullptr));
                                return;
                            }

                            auto* depthTex = res.GetTexture(data.Depth);
                            if (depthTex == nullptr) {
                                DebugAssert(false, TEXT("BasicDeferredRenderer"),
                                    "SkyBox skipped: depth texture is null (depthRef={}).",
                                    data.Depth.mId);
                                return;
                            }

                            auto* device = Rhi::RHIGetDevice();
                            DebugAssert(
                                device != nullptr, TEXT("BasicDeferredRenderer"), "Device lost");

                            Rhi::FRhiBindGroupDesc groupDesc{};
                            groupDesc.mLayout = shared.SkyBoxLayout.Get();

                            Rhi::FRhiBindGroupEntry cb{};
                            cb.mBinding = 0U;
                            cb.mType    = Rhi::ERhiBindingType::ConstantBuffer;
                            cb.mBuffer  = perFrameBuffer;
                            cb.mOffset  = 0ULL;
                            cb.mSize    = static_cast<u64>(sizeof(FPerFrameConstants));
                            groupDesc.mEntries.PushBack(cb);

                            Rhi::FRhiBindGroupEntry depth{};
                            depth.mBinding = MapSampledTextureBinding(0U);
                            depth.mType    = Rhi::ERhiBindingType::SampledTexture;
                            depth.mTexture = depthTex;
                            groupDesc.mEntries.PushBack(depth);

                            Rhi::FRhiBindGroupEntry sky{};
                            sky.mBinding = MapSampledTextureBinding(1U);
                            sky.mType    = Rhi::ERhiBindingType::SampledTexture;
                            sky.mTexture = skyCube;
                            groupDesc.mEntries.PushBack(sky);

                            Rhi::FRhiBindGroupEntry sampler{};
                            sampler.mBinding = MapSamplerBinding(0U);
                            sampler.mType    = Rhi::ERhiBindingType::Sampler;
                            sampler.mSampler = shared.OutputSampler.Get();
                            groupDesc.mEntries.PushBack(sampler);

                            auto bindGroup = device->CreateBindGroup(groupDesc);
                            if (!bindGroup) {
                                DebugAssert(false, TEXT("BasicDeferredRenderer"),
                                    "SkyBox skipped: CreateBindGroup failed.");
                                return;
                            }

                            ctx.RHISetGraphicsPipeline(shared.SkyBoxPipeline.Get());

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

                            ctx.RHISetBindGroup(0U, bindGroup.Get(), nullptr, 0U);
                            ctx.RHISetPrimitiveTopology(Rhi::ERhiPrimitiveTopology::TriangleList);
                            ctx.RHIDraw(3U, 1U, 0U, 0U);
                        });
                }
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
