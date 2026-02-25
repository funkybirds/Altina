
#include "Rendering/BasicDeferredRenderer.h"

#include "Rendering/DrawListExecutor.h"

#include "FrameGraph/FrameGraph.h"
#include "Lighting/LightTypes.h"
#include "Material/MaterialPass.h"
#include "Shadow/CascadedShadowMapping.h"
#include "View/ViewData.h"

#include "Rendering/PostProcess/PostProcess.h"
#include "Rendering/PostProcess/PostProcessSettings.h"

#include "Container/HashMap.h"
#include "Container/SmartPtr.h"
#include "Container/Vector.h"
#include "Logging/Log.h"
#include "Platform/Generic/GenericPlatformDecl.h"
#include "Rhi/Command/RhiCmdContext.h"
#include "Rhi/RhiBindGroup.h"
#include "Rhi/RhiBindGroupLayout.h"
#include "Rhi/RhiBuffer.h"
#include "Rhi/RhiDevice.h"
#include "Rhi/RhiInit.h"
#include "Rhi/RhiPipeline.h"
#include "Rhi/RhiPipelineLayout.h"
#include "Rhi/RhiSampler.h"
#include "Rhi/RhiTexture.h"

#include "Math/LinAlg/Common.h"

#include <algorithm>
#include <cmath>
#include <limits>

using AltinaEngine::Move;
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
            Core::Math::FMatrix4x4f ViewProjection;

            Core::Math::FMatrix4x4f View;
            Core::Math::FMatrix4x4f Proj;
            Core::Math::FMatrix4x4f ViewProj;
            Core::Math::FMatrix4x4f InvViewProj;

            f32                     ViewOriginWS[3] = { 0.0f, 0.0f, 0.0f };
            u32                     PointLightCount = 0U;

            f32                     DirLightDirectionWS[3] = { 0.0f, -1.0f, 0.0f };
            f32                     DirLightIntensity      = 0.0f;
            f32                     DirLightColor[3]       = { 1.0f, 1.0f, 1.0f };
            u32                     CSMCascadeCount        = 0U;

            // Keep these as explicit members (not an array). See DeferredLighting.hlsl:
            // Slang -> DXBC can mishandle row_major on matrix arrays, causing implicit transposes.
            Core::Math::FMatrix4x4f CSM_LightViewProj0{};
            Core::Math::FMatrix4x4f CSM_LightViewProj1{};
            Core::Math::FMatrix4x4f CSM_LightViewProj2{};
            Core::Math::FMatrix4x4f CSM_LightViewProj3{};
            f32                     CSM_SplitsVS[RenderCore::Shadow::kMaxCascades][4] = {};

            f32                     RenderTargetSize[2]    = { 0.0f, 0.0f };
            f32                     InvRenderTargetSize[2] = { 0.0f, 0.0f };

            u32                     bReverseZ           = 1U;
            u32                     DebugShadingMode    = 0U; // 0=PBR, 1=Lambert(debug)
            f32                     ShadowBias          = 0.0015f;
            f32                     _pad0               = 0.0f;
            f32                     ShadowMapInvSize[2] = { 0.0f, 0.0f };
            f32                     _pad1[2]            = { 0.0f, 0.0f };

            struct FPointLight {
                f32 PositionWS[3] = { 0.0f, 0.0f, 0.0f };
                f32 Range         = 0.0f;
                f32 Color[3]      = { 1.0f, 1.0f, 1.0f };
                f32 Intensity     = 0.0f;
            };
            FPointLight PointLights[kMaxPointLights]{};
        };

        struct FPerDrawConstants {
            Core::Math::FMatrix4x4f World;
            Core::Math::FMatrix4x4f NormalMatrix;
        };

        [[nodiscard]] auto ComputeNormalMatrix(const Core::Math::FMatrix4x4f& world) noexcept
            -> Core::Math::FMatrix4x4f {
            using Core::Math::FMatrix3x3f;
            using Core::Math::FMatrix4x4f;

            // Normal matrix = transpose(inverse(upper3x3(World))).
            FMatrix3x3f upper{};
            for (u32 r = 0U; r < 3U; ++r) {
                for (u32 c = 0U; c < 3U; ++c) {
                    upper(r, c) = world(r, c);
                }
            }

            const f32         det         = Core::Math::LinAlg::Determinant(upper);
            const bool        bInvertible = std::abs(det) > 1e-8f;

            const FMatrix3x3f normal3 = bInvertible
                ? Core::Math::Transpose(Core::Math::LinAlg::Inverse(upper))
                : Core::Math::LinAlg::Identity<f32, 3U>();

            FMatrix4x4f       normal4 = Core::Math::LinAlg::Identity<f32, 4U>();
            for (u32 r = 0U; r < 3U; ++r) {
                for (u32 c = 0U; c < 3U; ++c) {
                    normal4(r, c) = normal3(r, c);
                }
            }
            return normal4;
        }

        struct FWorldBoundsDebug {
            bool                  bValid        = false;
            Core::Math::FVector3f MinWS         = Core::Math::FVector3f(0.0f);
            Core::Math::FVector3f MaxWS         = Core::Math::FVector3f(0.0f);
            u32                   BatchCount    = 0U;
            u32                   InstanceCount = 0U;
        };

        [[nodiscard]] auto TransformAabbToWorld(const Core::Math::FMatrix4x4f& world,
            const RenderCore::Geometry::FStaticMeshBounds3f&                   localBounds,
            Core::Math::FVector3f& outMinWS, Core::Math::FVector3f& outMaxWS) -> bool {
            if (!localBounds.IsValid()) {
                return false;
            }

            using Core::Math::FVector3f;
            using Core::Math::FVector4f;

            const FVector3f& bmin = localBounds.Min;
            const FVector3f& bmax = localBounds.Max;

            FVector3f        minWS(std::numeric_limits<f32>::max());
            FVector3f        maxWS(-std::numeric_limits<f32>::max());

            const f32        xs[2] = { bmin[0], bmax[0] };
            const f32        ys[2] = { bmin[1], bmax[1] };
            const f32        zs[2] = { bmin[2], bmax[2] };

            for (u32 xi = 0U; xi < 2U; ++xi) {
                for (u32 yi = 0U; yi < 2U; ++yi) {
                    for (u32 zi = 0U; zi < 2U; ++zi) {
                        const auto p4 =
                            Core::Math::MatMul(world, FVector4f(xs[xi], ys[yi], zs[zi], 1.0f));
                        const f32 invW = (std::abs(p4[3]) > 1e-6f) ? (1.0f / p4[3]) : 1.0f;
                        const f32 x    = p4[0] * invW;
                        const f32 y    = p4[1] * invW;
                        const f32 z    = p4[2] * invW;
                        minWS[0]       = std::min(minWS[0], x);
                        minWS[1]       = std::min(minWS[1], y);
                        minWS[2]       = std::min(minWS[2], z);
                        maxWS[0]       = std::max(maxWS[0], x);
                        maxWS[1]       = std::max(maxWS[1], y);
                        maxWS[2]       = std::max(maxWS[2], z);
                    }
                }
            }

            outMinWS = minWS;
            outMaxWS = maxWS;
            return true;
        }

        [[nodiscard]] auto ComputeDrawListWorldBounds(const RenderCore::Render::FDrawList& list)
            -> FWorldBoundsDebug {
            FWorldBoundsDebug out{};
            out.BatchCount = static_cast<u32>(list.Batches.Size());

            Core::Math::FVector3f minWS(std::numeric_limits<f32>::max());
            Core::Math::FVector3f maxWS(-std::numeric_limits<f32>::max());

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
                    Core::Math::FVector3f instMinWS(0.0f);
                    Core::Math::FVector3f instMaxWS(0.0f);
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
            out.MinWS  = out.bValid ? minWS : Core::Math::FVector3f(0.0f);
            out.MaxWS  = out.bValid ? maxWS : Core::Math::FVector3f(0.0f);
            return out;
        }

        struct FDeferredSharedResources {
            RenderCore::FShaderRegistry                       Registry;
            RenderCore::FShaderRegistry::FShaderKey           OutputVSKey;
            RenderCore::FShaderRegistry::FShaderKey           OutputPSKey;
            RenderCore::FShaderRegistry::FShaderKey           LightingVSKey;
            RenderCore::FShaderRegistry::FShaderKey           LightingPSKey;
            RenderCore::FMaterialPassDesc                     DefaultPassDesc;
            RenderCore::FMaterialPassDesc                     DefaultShadowPassDesc;
            Container::TShared<RenderCore::FMaterialTemplate> DefaultTemplate;

            Rhi::FRhiBindGroupLayoutRef                       PerFrameLayout;
            Rhi::FRhiBindGroupLayoutRef                       PerDrawLayout;
            Rhi::FRhiBindGroupLayoutRef                       OutputLayout;
            Rhi::FRhiSamplerRef                               OutputSampler;
            Rhi::FRhiPipelineLayoutRef                        OutputPipelineLayout;
            Rhi::FRhiPipelineRef                              OutputPipeline;

            // Deferred lighting (FSQ -> BackBuffer).
            Rhi::FRhiBindGroupLayoutRef                       LightingLayout;
            Rhi::FRhiPipelineLayoutRef                        LightingPipelineLayout;
            Rhi::FRhiPipelineRef                              LightingPipeline;

            THashMap<u64, Rhi::FRhiPipelineRef>               BasePipelines;
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

            std::sort(outEntries.begin(), outEntries.end(), [](const auto& lhs, const auto& rhs) {
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

        void EnsureLayouts(Rhi::FRhiDevice& device, FDeferredSharedResources& resources) {
            if (!resources.PerFrameLayout) {
                Rhi::FRhiBindGroupLayoutEntry entry{};
                entry.mBinding    = 0U;
                entry.mType       = Rhi::ERhiBindingType::ConstantBuffer;
                entry.mVisibility = Rhi::ERhiShaderStageFlags::All;

                Rhi::FRhiBindGroupLayoutEntry samplerEntry{};
                samplerEntry.mBinding    = 0U;
                samplerEntry.mType       = Rhi::ERhiBindingType::Sampler;
                samplerEntry.mVisibility = Rhi::ERhiShaderStageFlags::All;

                Rhi::FRhiBindGroupLayoutDesc layoutDesc{};
                layoutDesc.mSetIndex = 0U;
                layoutDesc.mEntries.PushBack(entry);
                layoutDesc.mEntries.PushBack(samplerEntry);
                layoutDesc.mLayoutHash = BuildLayoutHash(layoutDesc.mEntries, layoutDesc.mSetIndex);
                resources.PerFrameLayout = device.CreateBindGroupLayout(layoutDesc);
            }

            if (!resources.PerDrawLayout) {
                Rhi::FRhiBindGroupLayoutEntry entry{};
                entry.mBinding    = 4U;
                entry.mType       = Rhi::ERhiBindingType::ConstantBuffer;
                entry.mVisibility = Rhi::ERhiShaderStageFlags::All;

                Rhi::FRhiBindGroupLayoutDesc layoutDesc{};
                layoutDesc.mSetIndex = 0U;
                layoutDesc.mEntries.PushBack(entry);
                layoutDesc.mLayoutHash = BuildLayoutHash(layoutDesc.mEntries, layoutDesc.mSetIndex);
                resources.PerDrawLayout = device.CreateBindGroupLayout(layoutDesc);
            }

            if (!resources.OutputLayout) {
                Rhi::FRhiBindGroupLayoutEntry samplerEntry{};
                samplerEntry.mBinding    = 0U;
                samplerEntry.mType       = Rhi::ERhiBindingType::Sampler;
                samplerEntry.mVisibility = Rhi::ERhiShaderStageFlags::All;

                Rhi::FRhiBindGroupLayoutDesc layoutDesc{};
                layoutDesc.mSetIndex = 0U;
                for (u32 binding = 0U; binding < 3U; ++binding) {
                    Rhi::FRhiBindGroupLayoutEntry textureEntry{};
                    textureEntry.mBinding    = binding;
                    textureEntry.mType       = Rhi::ERhiBindingType::SampledTexture;
                    textureEntry.mVisibility = Rhi::ERhiShaderStageFlags::All;
                    layoutDesc.mEntries.PushBack(textureEntry);
                }
                layoutDesc.mEntries.PushBack(samplerEntry);
                layoutDesc.mLayoutHash = BuildLayoutHash(layoutDesc.mEntries, layoutDesc.mSetIndex);
                resources.OutputLayout = device.CreateBindGroupLayout(layoutDesc);
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
                Rhi::FRhiBindGroupLayoutDesc layoutDesc{};
                layoutDesc.mSetIndex = 0U;

                // b0
                Rhi::FRhiBindGroupLayoutEntry cbuffer{};
                cbuffer.mBinding    = 0U;
                cbuffer.mType       = Rhi::ERhiBindingType::ConstantBuffer;
                cbuffer.mVisibility = Rhi::ERhiShaderStageFlags::All;
                layoutDesc.mEntries.PushBack(cbuffer);

                // t0..t4
                for (u32 binding = 0U; binding < 5U; ++binding) {
                    Rhi::FRhiBindGroupLayoutEntry texture{};
                    texture.mBinding    = binding;
                    texture.mType       = Rhi::ERhiBindingType::SampledTexture;
                    texture.mVisibility = Rhi::ERhiShaderStageFlags::All;
                    layoutDesc.mEntries.PushBack(texture);
                }

                // s0
                Rhi::FRhiBindGroupLayoutEntry sampler{};
                sampler.mBinding    = 0U;
                sampler.mType       = Rhi::ERhiBindingType::Sampler;
                sampler.mVisibility = Rhi::ERhiShaderStageFlags::All;
                layoutDesc.mEntries.PushBack(sampler);

                layoutDesc.mLayoutHash = BuildLayoutHash(layoutDesc.mEntries, layoutDesc.mSetIndex);
                resources.LightingLayout = device.CreateBindGroupLayout(layoutDesc);
            }

            if (!resources.LightingPipelineLayout) {
                Rhi::FRhiPipelineLayoutDesc layoutDesc{};
                if (resources.LightingLayout) {
                    layoutDesc.mBindGroupLayouts.PushBack(resources.LightingLayout.Get());
                }
                resources.LightingPipelineLayout = device.CreatePipelineLayout(layoutDesc);
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
                layoutDesc.mSetIndex   = 0U;
                layoutDesc.mEntries    = layoutEntries;
                layoutDesc.mLayoutHash = materialLayoutHash;
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

            const auto vs = data->Registry->FindShader(resolvedPass->Shaders.Vertex);
            const auto ps = data->Registry->FindShader(resolvedPass->Shaders.Pixel);
            if (!vs || !ps) {
                return nullptr;
            }

            Rhi::FRhiGraphicsPipelineDesc desc{};
            desc.mDebugName.Assign(TEXT("BasicDeferred.BasePassPipeline"));
            desc.mVertexShader   = vs.Get();
            desc.mPixelShader    = ps.Get();
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
            return ResolveBasePassPipeline(synthetic, nullptr, userData);
        }

        struct FBasePassBindingData {
            Rhi::FRhiBuffer*    PerDrawBuffer = nullptr;
            Rhi::FRhiBindGroup* PerDrawGroup  = nullptr;
            // D3D11 backend uses a single set index (0) and distinguishes resources by register.
            u32                 PerDrawSetIndex = 0U;
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
            constants.NormalMatrix = ComputeNormalMatrix(constants.World);
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

    auto FBasicDeferredRenderer::RegisterShader(
        const RenderCore::FShaderRegistry::FShaderKey& key, Rhi::FRhiShaderRef shader) -> bool {
        auto& resources = GetSharedResources();
        return resources.Registry.RegisterShader(key, Move(shader));
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

        if (!mPerFrameGroup && mPerFrameBuffer && resources.PerFrameLayout) {
            Rhi::FRhiBindGroupDesc groupDesc{};
            groupDesc.mLayout = resources.PerFrameLayout.Get();

            Rhi::FRhiBindGroupEntry entry{};
            entry.mBinding = 0U;
            entry.mType    = Rhi::ERhiBindingType::ConstantBuffer;
            entry.mBuffer  = mPerFrameBuffer.Get();
            entry.mOffset  = 0ULL;
            entry.mSize    = static_cast<u64>(sizeof(FPerFrameConstants));
            groupDesc.mEntries.PushBack(entry);

            if (resources.OutputSampler) {
                Rhi::FRhiBindGroupEntry samplerEntry{};
                samplerEntry.mBinding = 0U;
                samplerEntry.mType    = Rhi::ERhiBindingType::Sampler;
                samplerEntry.mSampler = resources.OutputSampler.Get();
                groupDesc.mEntries.PushBack(samplerEntry);
            }

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
                    entry.mBinding = 0U;
                    entry.mType    = Rhi::ERhiBindingType::ConstantBuffer;
                    entry.mBuffer  = mShadowPerFrameBuffers[i].Get();
                    entry.mOffset  = 0ULL;
                    entry.mSize    = static_cast<u64>(sizeof(FPerFrameConstants));
                    groupDesc.mEntries.PushBack(entry);

                    if (resources.OutputSampler) {
                        Rhi::FRhiBindGroupEntry samplerEntry{};
                        samplerEntry.mBinding = 0U;
                        samplerEntry.mType    = Rhi::ERhiBindingType::Sampler;
                        samplerEntry.mSampler = resources.OutputSampler.Get();
                        groupDesc.mEntries.PushBack(samplerEntry);
                    }

                    mShadowPerFrameGroups[i] = device.CreateBindGroup(groupDesc);
                }
            }
        }

        if (!mPerDrawBuffer) {
            Rhi::FRhiBufferDesc desc{};
            desc.mDebugName.Assign(TEXT("Deferred.PerDraw"));
            desc.mSizeBytes = sizeof(FPerDrawConstants);
            desc.mUsage     = Rhi::ERhiResourceUsage::Dynamic;
            desc.mBindFlags = Rhi::ERhiBufferBindFlags::Constant;
            desc.mCpuAccess = Rhi::ERhiCpuAccess::Write;
            mPerDrawBuffer  = device.CreateBuffer(desc);
        }

        if (!mPerDrawGroup && mPerDrawBuffer && resources.PerDrawLayout) {
            Rhi::FRhiBindGroupDesc groupDesc{};
            groupDesc.mLayout = resources.PerDrawLayout.Get();

            Rhi::FRhiBindGroupEntry entry{};
            entry.mBinding = 4U;
            entry.mType    = Rhi::ERhiBindingType::ConstantBuffer;
            entry.mBuffer  = mPerDrawBuffer.Get();
            entry.mOffset  = 0ULL;
            entry.mSize    = static_cast<u64>(sizeof(FPerDrawConstants));
            groupDesc.mEntries.PushBack(entry);

            mPerDrawGroup = device.CreateBindGroup(groupDesc);
        }
    }
    void FBasicDeferredRenderer::Render(RenderCore::FFrameGraph& graph) {
        const auto* view         = mViewContext.View;
        auto*       outputTarget = mViewContext.OutputTarget;
        const auto* drawList     = mViewContext.DrawList;
        if (view == nullptr || outputTarget == nullptr) {
            return;
        }

        if (!view->IsValid()) {
            return;
        }

        const u32 width  = view->RenderTargetExtent.Width;
        const u32 height = view->RenderTargetExtent.Height;
        if (width == 0U || height == 0U) {
            return;
        }

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
        drawBindings.PerFrame            = mPerFrameGroup.Get();
        drawBindings.PerFrameSetIndex    = 0U;
        drawBindings.PerDrawSetIndex     = 0U;
        drawBindings.PerMaterialSetIndex = 0U;

        FBasePassPipelineData pipelineData{};
        pipelineData.Device          = Rhi::RHIGetDevice();
        pipelineData.Registry        = &resources.Registry;
        pipelineData.PipelineCache   = &resources.BasePipelines;
        pipelineData.DefaultPassDesc = &resources.DefaultPassDesc;
        pipelineData.VertexLayout    = resources.BaseVertexLayout;

        FBasePassBindingData bindingData{};
        bindingData.PerDrawBuffer   = mPerDrawBuffer.Get();
        bindingData.PerDrawGroup    = mPerDrawGroup.Get();
        bindingData.PerDrawSetIndex = drawBindings.PerDrawSetIndex;

        const RenderCore::View::FViewRect viewRect = view->ViewRect;

        graph.AddPass<FBasePassData>(
            basePassDesc,
            [&](RenderCore::FFrameGraphPassBuilder& builder, FBasePassData& data) {
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
                const RenderCore::FFrameGraphPassResources&, const FBasePassData&) {
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
        csmSettings.CascadeCount = RenderCore::Shadow::kMaxCascades;
        // Keep this reasonably large; too small will clip all casters/receivers out of the
        // shadow frustum (common in demo scenes).
        csmSettings.MaxDistance   = 1000.0f;
        csmSettings.ShadowMapSize = 2048U;
        csmSettings.ReceiverBias  = 0.0015f;

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
                    const Core::Math::FVector3f bmin = bounds.MinWS;
                    const Core::Math::FVector3f bmax = bounds.MaxWS;

                    Core::Math::FVector4f       cornersWS[8] = {
                        Core::Math::FVector4f(bmin[0], bmin[1], bmin[2], 1.0f),
                        Core::Math::FVector4f(bmax[0], bmin[1], bmin[2], 1.0f),
                        Core::Math::FVector4f(bmax[0], bmax[1], bmin[2], 1.0f),
                        Core::Math::FVector4f(bmin[0], bmax[1], bmin[2], 1.0f),
                        Core::Math::FVector4f(bmin[0], bmin[1], bmax[2], 1.0f),
                        Core::Math::FVector4f(bmax[0], bmin[1], bmax[2], 1.0f),
                        Core::Math::FVector4f(bmax[0], bmax[1], bmax[2], 1.0f),
                        Core::Math::FVector4f(bmin[0], bmax[1], bmax[2], 1.0f),
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
            RenderCore::FFrameGraphPassDesc shadowPassDesc{};
            shadowPassDesc.mType  = RenderCore::EFrameGraphPassType::Raster;
            shadowPassDesc.mQueue = RenderCore::EFrameGraphQueue::Graphics;

            FBasePassPipelineData shadowPipelineData = pipelineData;
            shadowPipelineData.DefaultPassDesc       = &resources.DefaultShadowPassDesc;

            const u32 shadowSize = csmSettings.ShadowMapSize;
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
                        if (!shadowMap.IsValid()) {
                            RenderCore::FFrameGraphTextureDesc shadowDesc{};
                            shadowDesc.mDesc.mDebugName.Assign(TEXT("ShadowMap.CSM"));
                            shadowDesc.mDesc.mWidth       = shadowSize;
                            shadowDesc.mDesc.mHeight      = shadowSize;
                            shadowDesc.mDesc.mArrayLayers = csmData.CascadeCount;
                            shadowDesc.mDesc.mDimension   = (csmData.CascadeCount > 1U)
                                  ? Rhi::ERhiTextureDimension::Tex2DArray
                                  : Rhi::ERhiTextureDimension::Tex2D;
                            shadowDesc.mDesc.mFormat      = Rhi::ERhiFormat::D32Float;
                            shadowDesc.mDesc.mBindFlags   = Rhi::ERhiTextureBindFlags::DepthStencil
                                | Rhi::ERhiTextureBindFlags::ShaderResource;
                            shadowMap = builder.CreateTexture(shadowDesc);
                        }

                        data.Shadow = builder.Write(shadowMap, Rhi::ERhiResourceState::DepthWrite);

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
                        const FShadowPassData&) {
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

                data.GBufferA = gbufferA;
                data.GBufferB = gbufferB;
                data.GBufferC = gbufferC;
                data.Depth    = sceneDepth;

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
            [viewRect, view, lights, csmData, csmSettings, perFrameBuffer = mPerFrameBuffer.Get()](
                Rhi::FRhiCmdContext& ctx, const RenderCore::FFrameGraphPassResources& res,
                const FLightingPassData& data) {
                // LogInfo(TEXT("FG Pass: BasicDeferred.DeferredLighting"));
                auto& shared = GetSharedResources();
                if (!shared.LightingPipeline || !shared.LightingLayout || !shared.OutputSampler) {
                    return;
                }

                auto* texA      = res.GetTexture(data.GBufferA);
                auto* texB      = res.GetTexture(data.GBufferB);
                auto* texC      = res.GetTexture(data.GBufferC);
                auto* depthTex  = res.GetTexture(data.Depth);
                auto* shadowTex = res.GetTexture(data.Shadow);
                if (!texA || !texB || !texC || !depthTex || !shadowTex) {
                    return;
                }

                auto* device = Rhi::RHIGetDevice();
                if (!device) {
                    return;
                }

                if (perFrameBuffer == nullptr || view == nullptr) {
                    return;
                }

                // Fill per-frame constants (b0).
                FPerFrameConstants constants{};
                constants.ViewProjection = view->Matrices.ViewProjJittered;
                constants.View           = view->Matrices.View;
                constants.Proj           = view->Matrices.ProjJittered;
                constants.ViewProj       = view->Matrices.ViewProjJittered;
                constants.InvViewProj    = view->Matrices.InvViewProjJittered;

                constants.ViewOriginWS[0]  = view->ViewOrigin[0];
                constants.ViewOriginWS[1]  = view->ViewOrigin[1];
                constants.ViewOriginWS[2]  = view->ViewOrigin[2];
                constants.bReverseZ        = view->bReverseZ ? 1U : 0U;
                constants.DebugShadingMode = gDeferredLightingDebugShadingMode;

                const f32 w                   = static_cast<f32>(view->RenderTargetExtent.Width);
                const f32 h                   = static_cast<f32>(view->RenderTargetExtent.Height);
                constants.RenderTargetSize[0] = w;
                constants.RenderTargetSize[1] = h;
                constants.InvRenderTargetSize[0] = (w > 0.0f) ? (1.0f / w) : 0.0f;
                constants.InvRenderTargetSize[1] = (h > 0.0f) ? (1.0f / h) : 0.0f;

                // Lighting inputs.
                RenderCore::Lighting::FDirectionalLight dir{};
                if (lights != nullptr && lights->bHasMainDirectionalLight) {
                    dir = lights->MainDirectionalLight;
                } else {
                    dir.DirectionWS  = Core::Math::FVector3f(0.4f, 0.6f, 0.7f);
                    dir.Color        = Core::Math::FVector3f(1.0f, 1.0f, 1.0f);
                    dir.Intensity    = 2.0f;
                    dir.bCastShadows = false;
                }

                constants.DirLightDirectionWS[0] = dir.DirectionWS[0];
                constants.DirLightDirectionWS[1] = dir.DirectionWS[1];
                constants.DirLightDirectionWS[2] = dir.DirectionWS[2];
                constants.DirLightColor[0]       = dir.Color[0];
                constants.DirLightColor[1]       = dir.Color[1];
                constants.DirLightColor[2]       = dir.Color[2];
                constants.DirLightIntensity      = dir.Intensity;

                // Point lights.
                constants.PointLightCount = 0U;
                if (lights != nullptr && !lights->PointLights.IsEmpty()) {
                    const u32 count           = static_cast<u32>(lights->PointLights.Size());
                    const u32 clamped         = (count > kMaxPointLights) ? kMaxPointLights : count;
                    constants.PointLightCount = clamped;
                    for (u32 i = 0U; i < clamped; ++i) {
                        const auto& src                        = lights->PointLights[i];
                        constants.PointLights[i].PositionWS[0] = src.PositionWS[0];
                        constants.PointLights[i].PositionWS[1] = src.PositionWS[1];
                        constants.PointLights[i].PositionWS[2] = src.PositionWS[2];
                        constants.PointLights[i].Range         = src.Range;
                        constants.PointLights[i].Color[0]      = src.Color[0];
                        constants.PointLights[i].Color[1]      = src.Color[1];
                        constants.PointLights[i].Color[2]      = src.Color[2];
                        constants.PointLights[i].Intensity     = src.Intensity;
                    }
                }

                // CSM.
                constants.CSMCascadeCount = csmData.CascadeCount;
                constants.ShadowBias      = csmSettings.ReceiverBias;
                if (csmSettings.ShadowMapSize > 0U) {
                    const f32 inv = 1.0f / static_cast<f32>(csmSettings.ShadowMapSize);
                    constants.ShadowMapInvSize[0] = inv;
                    constants.ShadowMapInvSize[1] = inv;
                }
                for (u32 i = 0U; i < RenderCore::Shadow::kMaxCascades; ++i) {
                    constants.CSM_SplitsVS[i][0] = csmData.Cascades[i].SplitVS[0];
                    constants.CSM_SplitsVS[i][1] = csmData.Cascades[i].SplitVS[1];
                    constants.CSM_SplitsVS[i][2] = 0.0f;
                    constants.CSM_SplitsVS[i][3] = 0.0f;
                }
                // Avoid matrix arrays in the shared constant buffer (see FPerFrameConstants comment
                // above).
                constants.CSM_LightViewProj0 = csmData.Cascades[0].LightViewProj;
                constants.CSM_LightViewProj1 = csmData.Cascades[1].LightViewProj;
                constants.CSM_LightViewProj2 = csmData.Cascades[2].LightViewProj;
                constants.CSM_LightViewProj3 = csmData.Cascades[3].LightViewProj;

                ctx.RHIUpdateDynamicBufferDiscard(
                    perFrameBuffer, &constants, sizeof(constants), 0ULL);

                // Bind group (b0 + t0..t4 + s0).
                Rhi::FRhiBindGroupDesc groupDesc{};
                groupDesc.mLayout = shared.LightingLayout.Get();

                Rhi::FRhiBindGroupEntry cb{};
                cb.mBinding = 0U;
                cb.mType    = Rhi::ERhiBindingType::ConstantBuffer;
                cb.mBuffer  = perFrameBuffer;
                cb.mOffset  = 0ULL;
                cb.mSize    = static_cast<u64>(sizeof(FPerFrameConstants));
                groupDesc.mEntries.PushBack(cb);

                Rhi::FRhiBindGroupEntry eA{};
                eA.mBinding = 0U;
                eA.mType    = Rhi::ERhiBindingType::SampledTexture;
                eA.mTexture = texA;
                groupDesc.mEntries.PushBack(eA);

                Rhi::FRhiBindGroupEntry eB{};
                eB.mBinding = 1U;
                eB.mType    = Rhi::ERhiBindingType::SampledTexture;
                eB.mTexture = texB;
                groupDesc.mEntries.PushBack(eB);

                Rhi::FRhiBindGroupEntry eC{};
                eC.mBinding = 2U;
                eC.mType    = Rhi::ERhiBindingType::SampledTexture;
                eC.mTexture = texC;
                groupDesc.mEntries.PushBack(eC);

                Rhi::FRhiBindGroupEntry eDepth{};
                eDepth.mBinding = 3U;
                eDepth.mType    = Rhi::ERhiBindingType::SampledTexture;
                eDepth.mTexture = depthTex;
                groupDesc.mEntries.PushBack(eDepth);

                Rhi::FRhiBindGroupEntry eShadow{};
                eShadow.mBinding = 4U;
                eShadow.mType    = Rhi::ERhiBindingType::SampledTexture;
                eShadow.mTexture = shadowTex;
                groupDesc.mEntries.PushBack(eShadow);

                Rhi::FRhiBindGroupEntry sampler{};
                sampler.mBinding = 0U;
                sampler.mType    = Rhi::ERhiBindingType::Sampler;
                sampler.mSampler = shared.OutputSampler.Get();
                groupDesc.mEntries.PushBack(sampler);

                auto bindGroup = device->CreateBindGroup(groupDesc);
                if (!bindGroup) {
                    return;
                }

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

        // Post-process chain (stack + registry) -> Present.
        {
            FPostProcessStack pp{};
            pp.bEnable = (rPostProcessEnable.Get() != 0);

            const bool bEnableTaa = (rPostProcessTaa.Get() != 0);
            if (bEnableTaa) {
                FPostProcessNode node{};
                node.EffectId.Assign(TEXT("TAA"));
                node.bEnabled = true;
                node.Params[Container::FString(TEXT("Alpha"))] =
                    FPostProcessParamValue(rPostProcessTaaAlpha.Get());
                node.Params[Container::FString(TEXT("ClampK"))] =
                    FPostProcessParamValue(rPostProcessTaaClampK.Get());
                pp.Stack.PushBack(Move(node));
            }

            if (rPostProcessBloom.Get() != 0) {
                FPostProcessNode node{};
                node.EffectId.Assign(TEXT("Bloom"));
                node.bEnabled = true;
                // Defaults can be tuned via CVars; can be overridden later via stack params.
                node.Params[Container::FString(TEXT("Threshold"))] =
                    FPostProcessParamValue(rPostProcessBloomThreshold.Get());
                node.Params[Container::FString(TEXT("Knee"))] =
                    FPostProcessParamValue(rPostProcessBloomKnee.Get());
                node.Params[Container::FString(TEXT("Intensity"))] =
                    FPostProcessParamValue(rPostProcessBloomIntensity.Get());
                node.Params[Container::FString(TEXT("KawaseOffset"))] =
                    FPostProcessParamValue(rPostProcessBloomKawaseOffset.Get());
                node.Params[Container::FString(TEXT("Iterations"))] =
                    FPostProcessParamValue(rPostProcessBloomIterations.Get());
                pp.Stack.PushBack(Move(node));
            }

            if (rPostProcessTonemap.Get() != 0) {
                FPostProcessNode node{};
                node.EffectId.Assign(TEXT("Tonemap"));
                node.bEnabled = true;
                // Defaults; user can override via stack params later.
                node.Params[Container::FString(TEXT("Exposure"))] = FPostProcessParamValue(1.0f);
                node.Params[Container::FString(TEXT("Gamma"))]    = FPostProcessParamValue(2.2f);
                pp.Stack.PushBack(Move(node));
            }

            if (!bEnableTaa && rPostProcessFxaa.Get() != 0) {
                FPostProcessNode node{};
                node.EffectId.Assign(TEXT("Fxaa"));
                node.bEnabled = true;
                // Defaults can be tuned via CVars; can be overridden later via stack params.
                node.Params[Container::FString(TEXT("EdgeThreshold"))] =
                    FPostProcessParamValue(rPostProcessFxaaEdgeThreshold.Get());
                node.Params[Container::FString(TEXT("EdgeThresholdMin"))] =
                    FPostProcessParamValue(rPostProcessFxaaEdgeThresholdMin.Get());
                node.Params[Container::FString(TEXT("Subpix"))] =
                    FPostProcessParamValue(rPostProcessFxaaSubpix.Get());
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
