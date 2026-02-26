#include "Rendering/PostProcess/PostProcess.h"

#include "PostProcess/PostProcessResources.h"

#include "Logging/Log.h"
#include "Platform/Generic/GenericPlatformDecl.h"
#include "Rhi/Command/RhiCmdContext.h"
#include "Rhi/RhiBuffer.h"
#include "Rhi/RhiBindGroup.h"
#include "Rhi/RhiDevice.h"
#include "Rhi/RhiInit.h"
#include "Rhi/RhiPipeline.h"
#include "View/ViewData.h"

namespace AltinaEngine::Rendering::PostProcess::Builtin {
    namespace {
        using Detail::FBloomConstants;

        struct FBloomParams {
            f32 Threshold    = 1.0f;
            f32 Knee         = 0.5f;
            f32 Intensity    = 0.05f;
            f32 KawaseOffset = 1.0f;
            i32 Iterations   = 5;
        };

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

        void WriteBloomConstants(const FBloomParams& p) {
            auto& shared = Detail::GetPostProcessSharedResources();
            if (!shared.BloomConstantsBuffer) {
                return;
            }

            FBloomConstants constants{};
            constants.Threshold    = p.Threshold;
            constants.Knee         = p.Knee;
            constants.Intensity    = p.Intensity;
            constants.KawaseOffset = p.KawaseOffset;
            UpdateConstantBuffer(shared.BloomConstantsBuffer.Get(), &constants, sizeof(constants));
        }

        void SetViewportScissor(Rhi::FRhiCmdContext& cmd, u32 width, u32 height) {
            Rhi::FRhiViewportRect viewport{};
            viewport.mX        = 0.0f;
            viewport.mY        = 0.0f;
            viewport.mWidth    = static_cast<f32>(width);
            viewport.mHeight   = static_cast<f32>(height);
            viewport.mMinDepth = 0.0f;
            viewport.mMaxDepth = 1.0f;
            cmd.RHISetViewport(viewport);

            Rhi::FRhiScissorRect scissor{};
            scissor.mX      = 0;
            scissor.mY      = 0;
            scissor.mWidth  = width;
            scissor.mHeight = height;
            cmd.RHISetScissor(scissor);
        }

        [[nodiscard]] auto FindParamValue(const FPostProcessParams& params,
            Container::FStringView name) -> const FPostProcessParamValue* {
            for (const auto& it : params) {
                if (it.first.ToView() == name) {
                    return &it.second;
                }
            }
            return nullptr;
        }

        [[nodiscard]] auto GetParamF32(const FPostProcessParams& params,
            Container::FStringView name, f32 defaultValue) -> f32 {
            const auto* v = FindParamValue(params, name);
            if (v == nullptr) {
                return defaultValue;
            }
            if (const auto* p = v->TryGet<f32>()) {
                return *p;
            }
            if (const auto* p = v->TryGet<i32>()) {
                return static_cast<f32>(*p);
            }
            if (const auto* p = v->TryGet<bool>()) {
                return *p ? 1.0f : 0.0f;
            }
            return defaultValue;
        }

        [[nodiscard]] auto GetParamI32(const FPostProcessParams& params,
            Container::FStringView name, i32 defaultValue) -> i32 {
            const auto* v = FindParamValue(params, name);
            if (v == nullptr) {
                return defaultValue;
            }
            if (const auto* p = v->TryGet<i32>()) {
                return *p;
            }
            if (const auto* p = v->TryGet<f32>()) {
                return static_cast<i32>(*p);
            }
            if (const auto* p = v->TryGet<bool>()) {
                return *p ? 1 : 0;
            }
            return defaultValue;
        }

        [[nodiscard]] auto MakeBindGroup(Rhi::FRhiDevice& device,
            const Detail::FPostProcessSharedResources& shared, Rhi::FRhiTexture* inTex)
            -> Rhi::FRhiBindGroupRef {
            Rhi::FRhiBindGroupDesc groupDesc{};
            groupDesc.mLayout = shared.Layout.Get();

            // b0
            Rhi::FRhiBindGroupEntry cb{};
            cb.mBinding = 0U;
            cb.mType    = Rhi::ERhiBindingType::ConstantBuffer;
            cb.mBuffer  = shared.BloomConstantsBuffer.Get();
            cb.mOffset  = 0ULL;
            cb.mSize    = static_cast<u64>(sizeof(FBloomConstants));
            groupDesc.mEntries.PushBack(cb);

            // t0
            Rhi::FRhiBindGroupEntry tex{};
            tex.mBinding = 0U;
            tex.mType    = Rhi::ERhiBindingType::SampledTexture;
            tex.mTexture = inTex;
            groupDesc.mEntries.PushBack(tex);

            // s0
            Rhi::FRhiBindGroupEntry sampler{};
            sampler.mBinding = 0U;
            sampler.mType    = Rhi::ERhiBindingType::Sampler;
            sampler.mSampler = shared.LinearSampler.Get();
            groupDesc.mEntries.PushBack(sampler);

            return device.CreateBindGroup(groupDesc);
        }

        [[nodiscard]] auto MakeBloomTextureDesc(u32 width, u32 height, u32 level)
            -> RenderCore::FFrameGraphTextureDesc {
            RenderCore::FFrameGraphTextureDesc desc{};
            desc.mDesc.mDebugName.Assign(TEXT("PostProcess.Bloom.Down"));
            desc.mDesc.mDebugName.AppendNumber(level);
            desc.mDesc.mWidth       = width;
            desc.mDesc.mHeight      = height;
            desc.mDesc.mArrayLayers = 1U;
            desc.mDesc.mFormat      = Rhi::ERhiFormat::R16G16B16A16Float;
            desc.mDesc.mBindFlags =
                Rhi::ERhiTextureBindFlags::RenderTarget | Rhi::ERhiTextureBindFlags::ShaderResource;
            return desc;
        }
    } // namespace

    void AddBloom(RenderCore::FFrameGraph& graph, const RenderCore::View::FViewData& view,
        const FPostProcessNode& node, const FPostProcessBuildContext& ctx, FPostProcessIO& io) {
        (void)ctx;

        if (!io.SceneColor.IsValid()) {
            return;
        }

        if (!Detail::EnsurePostProcessSharedResources()) {
            static bool sLoggedOnce = false;
            if (!sLoggedOnce) {
                sLoggedOnce = true;
                LogWarning(TEXT(
                    "PostProcess.Bloom skipped: shared resources not ready (shader/pipeline creation failed?)"));
            }
            return;
        }

        FBloomParams params{};
        params.Threshold    = GetParamF32(node.Params, TEXT("Threshold"), 1.0f);
        params.Knee         = GetParamF32(node.Params, TEXT("Knee"), 0.5f);
        params.Intensity    = GetParamF32(node.Params, TEXT("Intensity"), 0.05f);
        params.KawaseOffset = GetParamF32(node.Params, TEXT("KawaseOffset"), 1.0f);
        params.Iterations   = GetParamI32(node.Params, TEXT("Iterations"), 5);

        if (params.Iterations < 1) {
            params.Iterations = 1;
        }
        if (params.Iterations > 8) {
            params.Iterations = 8;
        }

        const u32                         baseW = view.RenderTargetExtent.Width;
        const u32                         baseH = view.RenderTargetExtent.Height;

        static constexpr u32              kMaxLevels = 8U;
        RenderCore::FFrameGraphTextureRef down[kMaxLevels]{};

        const u32                         levelCount = static_cast<u32>(params.Iterations);

        // Prefilter + downsample: scene -> down[0] (half-res).
        {
            struct FPassData {
                RenderCore::FFrameGraphTextureRef In;
                RenderCore::FFrameGraphTextureRef Out;
                RenderCore::FFrameGraphRTVRef     OutRTV;
                FBloomParams                      Params;
                u32                               Width  = 1U;
                u32                               Height = 1U;
            };

            RenderCore::FFrameGraphPassDesc desc{};
            desc.mName  = "PostProcess.Bloom.Prefilter";
            desc.mType  = RenderCore::EFrameGraphPassType::Raster;
            desc.mQueue = RenderCore::EFrameGraphQueue::Graphics;

            graph.AddPass<FPassData>(
                desc,
                [&](RenderCore::FFrameGraphPassBuilder& builder, FPassData& data) {
                    data.Params = params;
                    data.Width  = (baseW >> 1U) > 0U ? (baseW >> 1U) : 1U;
                    data.Height = (baseH >> 1U) > 0U ? (baseH >> 1U) : 1U;

                    data.In = builder.Read(io.SceneColor, Rhi::ERhiResourceState::ShaderResource);
                    data.Out =
                        builder.CreateTexture(MakeBloomTextureDesc(data.Width, data.Height, 0U));
                    data.Out = builder.Write(data.Out, Rhi::ERhiResourceState::RenderTarget);
                    down[0]  = data.Out;

                    Rhi::FRhiTextureViewRange viewRange{};
                    viewRange.mMipCount        = 1U;
                    viewRange.mLayerCount      = 1U;
                    viewRange.mDepthSliceCount = 1U;

                    Rhi::FRhiRenderTargetViewDesc rtvDesc{};
                    rtvDesc.mDebugName.Assign(TEXT("PostProcess.Bloom.Down0.RTV"));
                    rtvDesc.mFormat = Rhi::ERhiFormat::R16G16B16A16Float;
                    rtvDesc.mRange  = viewRange;
                    data.OutRTV     = builder.CreateRTV(data.Out, rtvDesc);

                    RenderCore::FRdgRenderTargetBinding rtv{};
                    rtv.mRTV = data.OutRTV;
                    builder.SetRenderTargets(&rtv, 1U, nullptr);
                },
                [](Rhi::FRhiCmdContext& cmd, const RenderCore::FFrameGraphPassResources& res,
                    const FPassData& data) {
                    auto& shared = Detail::GetPostProcessSharedResources();
                    if (!shared.BloomPrefilterPipeline || !shared.Layout || !shared.LinearSampler
                        || !shared.BloomConstantsBuffer) {
                        return;
                    }

                    auto* inTex  = res.GetTexture(data.In);
                    auto* device = Rhi::RHIGetDevice();
                    if (!inTex || device == nullptr) {
                        return;
                    }

                    WriteBloomConstants(data.Params);

                    auto bindGroup = MakeBindGroup(*device, shared, inTex);
                    if (!bindGroup) {
                        return;
                    }

                    SetViewportScissor(cmd, data.Width, data.Height);
                    cmd.RHISetGraphicsPipeline(shared.BloomPrefilterPipeline.Get());
                    cmd.RHISetBindGroup(0U, bindGroup.Get(), nullptr, 0U);
                    cmd.RHISetPrimitiveTopology(Rhi::ERhiPrimitiveTopology::TriangleList);
                    cmd.RHIDraw(3U, 1U, 0U, 0U);
                });
        }

        // Downsample chain: down[i-1] -> down[i]
        for (u32 i = 1U; i < levelCount; ++i) {
            struct FPassData {
                RenderCore::FFrameGraphTextureRef In;
                RenderCore::FFrameGraphTextureRef Out;
                RenderCore::FFrameGraphRTVRef     OutRTV;
                FBloomParams                      Params;
                u32                               Width  = 1U;
                u32                               Height = 1U;
                u32                               Level  = 0U;
            };

            RenderCore::FFrameGraphPassDesc desc{};
            desc.mName  = "PostProcess.Bloom.Downsample";
            desc.mType  = RenderCore::EFrameGraphPassType::Raster;
            desc.mQueue = RenderCore::EFrameGraphQueue::Graphics;

            graph.AddPass<FPassData>(
                desc,
                [&](RenderCore::FFrameGraphPassBuilder& builder, FPassData& data) {
                    data.Params = params;
                    data.Level  = i;
                    data.Width  = (baseW >> (i + 1U)) > 0U ? (baseW >> (i + 1U)) : 1U;
                    data.Height = (baseH >> (i + 1U)) > 0U ? (baseH >> (i + 1U)) : 1U;

                    data.In = builder.Read(down[i - 1U], Rhi::ERhiResourceState::ShaderResource);
                    data.Out =
                        builder.CreateTexture(MakeBloomTextureDesc(data.Width, data.Height, i));
                    data.Out = builder.Write(data.Out, Rhi::ERhiResourceState::RenderTarget);
                    down[i]  = data.Out;

                    Rhi::FRhiTextureViewRange viewRange{};
                    viewRange.mMipCount        = 1U;
                    viewRange.mLayerCount      = 1U;
                    viewRange.mDepthSliceCount = 1U;

                    Rhi::FRhiRenderTargetViewDesc rtvDesc{};
                    rtvDesc.mDebugName.Assign(TEXT("PostProcess.Bloom.Down.RTV"));
                    rtvDesc.mFormat = Rhi::ERhiFormat::R16G16B16A16Float;
                    rtvDesc.mRange  = viewRange;
                    data.OutRTV     = builder.CreateRTV(data.Out, rtvDesc);

                    RenderCore::FRdgRenderTargetBinding rtv{};
                    rtv.mRTV = data.OutRTV;
                    builder.SetRenderTargets(&rtv, 1U, nullptr);
                },
                [](Rhi::FRhiCmdContext& cmd, const RenderCore::FFrameGraphPassResources& res,
                    const FPassData& data) {
                    auto& shared = Detail::GetPostProcessSharedResources();
                    if (!shared.BloomDownsamplePipeline || !shared.Layout || !shared.LinearSampler
                        || !shared.BloomConstantsBuffer) {
                        return;
                    }

                    auto* inTex  = res.GetTexture(data.In);
                    auto* device = Rhi::RHIGetDevice();
                    if (!inTex || device == nullptr) {
                        return;
                    }

                    WriteBloomConstants(data.Params);

                    auto bindGroup = MakeBindGroup(*device, shared, inTex);
                    if (!bindGroup) {
                        return;
                    }

                    SetViewportScissor(cmd, data.Width, data.Height);
                    cmd.RHISetGraphicsPipeline(shared.BloomDownsamplePipeline.Get());
                    cmd.RHISetBindGroup(0U, bindGroup.Get(), nullptr, 0U);
                    cmd.RHISetPrimitiveTopology(Rhi::ERhiPrimitiveTopology::TriangleList);
                    cmd.RHIDraw(3U, 1U, 0U, 0U);
                });
        }

        // Upsample chain: down[i] += upsample(down[i+1]) using additive blending.
        // Note: We reuse the existing down textures as the accumulation targets.
        for (i32 i = static_cast<i32>(levelCount) - 2; i >= 0; --i) {
            struct FPassData {
                RenderCore::FFrameGraphTextureRef InLow;
                RenderCore::FFrameGraphTextureRef OutHigh;
                RenderCore::FFrameGraphRTVRef     OutRTV;
                FBloomParams                      Params;
                u32                               Width  = 1U;
                u32                               Height = 1U;
                i32                               Level  = 0;
            };

            RenderCore::FFrameGraphPassDesc desc{};
            desc.mName  = "PostProcess.Bloom.UpsampleAdd";
            desc.mType  = RenderCore::EFrameGraphPassType::Raster;
            desc.mQueue = RenderCore::EFrameGraphQueue::Graphics;

            graph.AddPass<FPassData>(
                desc,
                [&](RenderCore::FFrameGraphPassBuilder& builder, FPassData& data) {
                    data.Params  = params;
                    data.Level   = i;
                    const u32 ui = static_cast<u32>(i);
                    data.Width   = (baseW >> (ui + 1U)) > 0U ? (baseW >> (ui + 1U)) : 1U;
                    data.Height  = (baseH >> (ui + 1U)) > 0U ? (baseH >> (ui + 1U)) : 1U;

                    data.InLow =
                        builder.Read(down[ui + 1U], Rhi::ERhiResourceState::ShaderResource);
                    data.OutHigh = builder.Write(down[ui], Rhi::ERhiResourceState::RenderTarget);

                    Rhi::FRhiTextureViewRange viewRange{};
                    viewRange.mMipCount        = 1U;
                    viewRange.mLayerCount      = 1U;
                    viewRange.mDepthSliceCount = 1U;

                    Rhi::FRhiRenderTargetViewDesc rtvDesc{};
                    rtvDesc.mDebugName.Assign(TEXT("PostProcess.Bloom.UpAdd.RTV"));
                    rtvDesc.mFormat = Rhi::ERhiFormat::R16G16B16A16Float;
                    rtvDesc.mRange  = viewRange;
                    data.OutRTV     = builder.CreateRTV(data.OutHigh, rtvDesc);

                    RenderCore::FRdgRenderTargetBinding rtv{};
                    rtv.mRTV     = data.OutRTV;
                    rtv.mLoadOp  = Rhi::ERhiLoadOp::Load; // keep down[i] as the base
                    rtv.mStoreOp = Rhi::ERhiStoreOp::Store;
                    builder.SetRenderTargets(&rtv, 1U, nullptr);
                },
                [](Rhi::FRhiCmdContext& cmd, const RenderCore::FFrameGraphPassResources& res,
                    const FPassData& data) {
                    auto& shared = Detail::GetPostProcessSharedResources();
                    if (!shared.BloomUpsampleAddPipeline || !shared.Layout || !shared.LinearSampler
                        || !shared.BloomConstantsBuffer) {
                        return;
                    }

                    auto* inTex  = res.GetTexture(data.InLow);
                    auto* device = Rhi::RHIGetDevice();
                    if (!inTex || device == nullptr) {
                        return;
                    }

                    WriteBloomConstants(data.Params);

                    auto bindGroup = MakeBindGroup(*device, shared, inTex);
                    if (!bindGroup) {
                        return;
                    }

                    SetViewportScissor(cmd, data.Width, data.Height);
                    cmd.RHISetGraphicsPipeline(shared.BloomUpsampleAddPipeline.Get());
                    cmd.RHISetBindGroup(0U, bindGroup.Get(), nullptr, 0U);
                    cmd.RHISetPrimitiveTopology(Rhi::ERhiPrimitiveTopology::TriangleList);
                    cmd.RHIDraw(3U, 1U, 0U, 0U);
                });
        }

        // Apply bloom: SceneColor += BloomApply(down[0]) using additive blending.
        {
            struct FPassData {
                RenderCore::FFrameGraphTextureRef InBloom;
                RenderCore::FFrameGraphTextureRef OutScene;
                RenderCore::FFrameGraphRTVRef     OutRTV;
                FBloomParams                      Params;
                u32                               Width  = 1U;
                u32                               Height = 1U;
            };

            RenderCore::FFrameGraphPassDesc desc{};
            desc.mName  = "PostProcess.Bloom.Apply";
            desc.mType  = RenderCore::EFrameGraphPassType::Raster;
            desc.mQueue = RenderCore::EFrameGraphQueue::Graphics;

            graph.AddPass<FPassData>(
                desc,
                [&](RenderCore::FFrameGraphPassBuilder& builder, FPassData& data) {
                    data.Params = params;
                    data.Width  = baseW;
                    data.Height = baseH;

                    data.InBloom = builder.Read(down[0], Rhi::ERhiResourceState::ShaderResource);
                    // IMPORTANT:
                    // This pass uses LoadOp=Load to additively blend bloom onto the existing
                    // SceneColor. Declare a read dependency so the frame graph preserves the
                    // prior contents produced by previous passes (lighting/skybox/etc.).
                    (void)builder.Read(io.SceneColor, Rhi::ERhiResourceState::RenderTarget);
                    data.OutScene =
                        builder.Write(io.SceneColor, Rhi::ERhiResourceState::RenderTarget);

                    Rhi::FRhiTextureViewRange viewRange{};
                    viewRange.mMipCount        = 1U;
                    viewRange.mLayerCount      = 1U;
                    viewRange.mDepthSliceCount = 1U;

                    Rhi::FRhiRenderTargetViewDesc rtvDesc{};
                    rtvDesc.mDebugName.Assign(TEXT("PostProcess.SceneColorHDR.BloomApply.RTV"));
                    rtvDesc.mFormat = Rhi::ERhiFormat::R16G16B16A16Float;
                    rtvDesc.mRange  = viewRange;
                    data.OutRTV     = builder.CreateRTV(data.OutScene, rtvDesc);

                    RenderCore::FRdgRenderTargetBinding rtv{};
                    rtv.mRTV     = data.OutRTV;
                    rtv.mLoadOp  = Rhi::ERhiLoadOp::Load;
                    rtv.mStoreOp = Rhi::ERhiStoreOp::Store;
                    builder.SetRenderTargets(&rtv, 1U, nullptr);
                },
                [](Rhi::FRhiCmdContext& cmd, const RenderCore::FFrameGraphPassResources& res,
                    const FPassData& data) {
                    auto& shared = Detail::GetPostProcessSharedResources();
                    if (!shared.BloomApplyAddPipeline || !shared.Layout || !shared.LinearSampler
                        || !shared.BloomConstantsBuffer) {
                        return;
                    }

                    auto* inTex  = res.GetTexture(data.InBloom);
                    auto* device = Rhi::RHIGetDevice();
                    if (!inTex || device == nullptr) {
                        return;
                    }

                    WriteBloomConstants(data.Params);

                    auto bindGroup = MakeBindGroup(*device, shared, inTex);
                    if (!bindGroup) {
                        return;
                    }

                    SetViewportScissor(cmd, data.Width, data.Height);
                    cmd.RHISetGraphicsPipeline(shared.BloomApplyAddPipeline.Get());
                    cmd.RHISetBindGroup(0U, bindGroup.Get(), nullptr, 0U);
                    cmd.RHISetPrimitiveTopology(Rhi::ERhiPrimitiveTopology::TriangleList);
                    cmd.RHIDraw(3U, 1U, 0U, 0U);
                });
        }
    }
} // namespace AltinaEngine::Rendering::PostProcess::Builtin
