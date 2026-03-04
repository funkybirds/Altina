#include "Rendering/PostProcess/PostProcess.h"

#include "PostProcess/PostProcessResources.h"
#include "TemporalAA/TemporalAAState.h"

#include "Logging/Log.h"
#include "Platform/Generic/GenericPlatformDecl.h"
#include "Rhi/Command/RhiCmdContext.h"
#include "Rhi/RhiBuffer.h"
#include "Rhi/RhiBindGroup.h"
#include "Rhi/RhiDevice.h"
#include "Rhi/RhiInit.h"
#include "Rhi/RhiPipeline.h"
#include "Rhi/RhiTexture.h"
#include "View/ViewData.h"

namespace AltinaEngine::Rendering::PostProcess::Builtin {
    namespace {
        using Detail::FTaaConstants;

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
    } // namespace

    void AddTaa(RenderCore::FFrameGraph& graph, const RenderCore::View::FViewData& view,
        const FPostProcessNode& node, const FPostProcessBuildContext& ctx, FPostProcessIO& io) {
        if (!io.SceneColor.IsValid() || !io.Depth.IsValid()) {
            return;
        }

        if (ctx.ViewKey == 0ULL) {
            static bool sLoggedOnce = false;
            if (!sLoggedOnce) {
                sLoggedOnce = true;
                LogWarning(TEXT("PostProcess.TAA skipped: missing ViewKey (no persistent state)."));
            }
            return;
        }

        if (!Detail::EnsurePostProcessSharedResources()) {
            static bool sLoggedOnce = false;
            if (!sLoggedOnce) {
                sLoggedOnce = true;
                LogWarning(TEXT(
                    "PostProcess.TAA skipped: shared resources not ready (shader/pipeline creation failed?)"));
            }
            return;
        }

        auto& shared = Detail::GetPostProcessSharedResources();
        if (!shared.TaaPipeline || !shared.TaaLayout || !shared.LinearSampler
            || !shared.TaaConstantsBuffer) {
            return;
        }

        auto* device = Rhi::RHIGetDevice();
        if (!device) {
            return;
        }

        const u32 width  = view.RenderTargetExtent.Width;
        const u32 height = view.RenderTargetExtent.Height;
        if (width == 0U || height == 0U) {
            return;
        }

        // Ensure external history textures exist (allocated on render thread).
        constexpr auto kHistoryFormat = Rhi::ERhiFormat::R16G16B16A16Float;
        TemporalAA::Detail::EnsureHistoryTextures(
            *device, ctx.ViewKey, width, height, kHistoryFormat);

        const auto historyRead  = TemporalAA::Detail::GetHistoryReadTexture(ctx.ViewKey);
        const auto historyWrite = TemporalAA::Detail::GetHistoryWriteTexture(ctx.ViewKey);
        if (!historyRead || !historyWrite) {
            return;
        }

        const auto historyReadImported =
            graph.ImportTexture(historyRead.Get(), Rhi::ERhiResourceState::ShaderResource);
        const auto historyWriteImported =
            graph.ImportTexture(historyWrite.Get(), Rhi::ERhiResourceState::RenderTarget);

        // Params.
        const f32                         alpha  = GetParamF32(node.Params, TEXT("Alpha"), 0.9f);
        const f32                         clampK = GetParamF32(node.Params, TEXT("ClampK"), 1.0f);

        RenderCore::FFrameGraphTextureRef outRef{};

        struct FPassData {
            RenderCore::FFrameGraphTextureRef Current;
            RenderCore::FFrameGraphTextureRef History;
            RenderCore::FFrameGraphTextureRef Depth;
            RenderCore::FFrameGraphTextureRef Out;
            RenderCore::FFrameGraphRTVRef     OutRTV;
            f32                               Alpha  = 0.9f;
            f32                               ClampK = 1.0f;
        };

        RenderCore::FFrameGraphPassDesc desc{};
        desc.mName  = "PostProcess.TAA";
        desc.mType  = RenderCore::EFrameGraphPassType::Raster;
        desc.mQueue = RenderCore::EFrameGraphQueue::Graphics;

        graph.AddPass<FPassData>(
            desc,
            [&](RenderCore::FFrameGraphPassBuilder& builder, FPassData& data) {
                data.Alpha  = alpha;
                data.ClampK = clampK;

                data.Current = builder.Read(io.SceneColor, Rhi::ERhiResourceState::ShaderResource);
                data.Depth   = builder.Read(io.Depth, Rhi::ERhiResourceState::ShaderResource);

                data.History =
                    builder.Read(historyReadImported, Rhi::ERhiResourceState::ShaderResource);

                data.Out = historyWriteImported;
                data.Out = builder.Write(data.Out, Rhi::ERhiResourceState::RenderTarget);
                outRef   = data.Out;

                Rhi::FRhiTextureViewRange viewRange{};
                viewRange.mMipCount        = 1U;
                viewRange.mLayerCount      = 1U;
                viewRange.mDepthSliceCount = 1U;

                Rhi::FRhiRenderTargetViewDesc rtvDesc{};
                rtvDesc.mDebugName.Assign(TEXT("PostProcess.TAA.Out.RTV"));
                rtvDesc.mFormat = kHistoryFormat;
                rtvDesc.mRange  = viewRange;
                data.OutRTV     = builder.CreateRTV(data.Out, rtvDesc);

                RenderCore::FRdgRenderTargetBinding rtv{};
                rtv.mRTV = data.OutRTV;
                builder.SetRenderTargets(&rtv, 1U, nullptr);

                // Keep history alive even if subsequent nodes are disabled.
                builder.SetSideEffect();
            },
            [viewRect = view.ViewRect, viewKey = ctx.ViewKey, viewMatrices = view.Matrices,
                prev = view.Previous](Rhi::FRhiCmdContext&  cmd,
                const RenderCore::FFrameGraphPassResources& res, const FPassData& data) {
                auto& shared = Detail::GetPostProcessSharedResources();
                if (!shared.TaaPipeline || !shared.TaaLayout || !shared.LinearSampler
                    || !shared.TaaConstantsBuffer) {
                    return;
                }

                auto* currentTex = res.GetTexture(data.Current);
                auto* historyTex = res.GetTexture(data.History);
                auto* depthTex   = res.GetTexture(data.Depth);
                auto* device     = Rhi::RHIGetDevice();
                if (!currentTex || !historyTex || !depthTex || device == nullptr) {
                    return;
                }

                FTaaConstants constants{};
                constants.RenderTargetSize[0] = static_cast<f32>(viewRect.Width);
                constants.RenderTargetSize[1] = static_cast<f32>(viewRect.Height);
                constants.InvRenderTargetSize[0] =
                    (viewRect.Width > 0U) ? (1.0f / static_cast<f32>(viewRect.Width)) : 0.0f;
                constants.InvRenderTargetSize[1] =
                    (viewRect.Height > 0U) ? (1.0f / static_cast<f32>(viewRect.Height)) : 0.0f;

                constants.JitterNdc[0]     = viewMatrices.JitterNdc[0];
                constants.JitterNdc[1]     = viewMatrices.JitterNdc[1];
                constants.PrevJitterNdc[0] = prev.Matrices.JitterNdc[0];
                constants.PrevJitterNdc[1] = prev.Matrices.JitterNdc[1];

                const bool bHasHistory = prev.bHasValidHistory && !prev.bCameraCut;
                constants.bHasHistory  = bHasHistory ? 1U : 0U;

                // On invalid history, bypass (weight=0).
                constants.Alpha  = bHasHistory ? data.Alpha : 0.0f;
                constants.ClampK = data.ClampK;

                constants.InvViewProjJittered  = viewMatrices.InvViewProjJittered;
                constants.PrevViewProjJittered = prev.Matrices.ViewProjJittered;

                UpdateConstantBuffer(
                    shared.TaaConstantsBuffer.Get(), &constants, sizeof(constants));

                Rhi::FRhiBindGroupDesc groupDesc{};
                groupDesc.mLayout = shared.TaaLayout.Get();

                // b0
                Rhi::FRhiBindGroupEntry cb{};
                cb.mBinding = 0U;
                cb.mType    = Rhi::ERhiBindingType::ConstantBuffer;
                cb.mBuffer  = shared.TaaConstantsBuffer.Get();
                cb.mOffset  = 0ULL;
                cb.mSize    = static_cast<u64>(sizeof(FTaaConstants));
                groupDesc.mEntries.PushBack(cb);

                // t0
                Rhi::FRhiBindGroupEntry t0{};
                t0.mBinding = Detail::MapSampledTextureBinding(0U);
                t0.mType    = Rhi::ERhiBindingType::SampledTexture;
                t0.mTexture = currentTex;
                groupDesc.mEntries.PushBack(t0);

                // t1
                Rhi::FRhiBindGroupEntry t1{};
                t1.mBinding = Detail::MapSampledTextureBinding(1U);
                t1.mType    = Rhi::ERhiBindingType::SampledTexture;
                t1.mTexture = historyTex;
                groupDesc.mEntries.PushBack(t1);

                // t2
                Rhi::FRhiBindGroupEntry t2{};
                t2.mBinding = Detail::MapSampledTextureBinding(2U);
                t2.mType    = Rhi::ERhiBindingType::SampledTexture;
                t2.mTexture = depthTex;
                groupDesc.mEntries.PushBack(t2);

                // s0
                Rhi::FRhiBindGroupEntry sampler{};
                sampler.mBinding = Detail::MapSamplerBinding(0U);
                sampler.mType    = Rhi::ERhiBindingType::Sampler;
                sampler.mSampler = shared.LinearSampler.Get();
                groupDesc.mEntries.PushBack(sampler);

                auto bindGroup = device->CreateBindGroup(groupDesc);
                if (!bindGroup) {
                    return;
                }

                cmd.RHISetGraphicsPipeline(shared.TaaPipeline.Get());

                Rhi::FRhiViewportRect viewport{};
                viewport.mX        = static_cast<f32>(viewRect.X);
                viewport.mY        = static_cast<f32>(viewRect.Y);
                viewport.mWidth    = static_cast<f32>(viewRect.Width);
                viewport.mHeight   = static_cast<f32>(viewRect.Height);
                viewport.mMinDepth = 0.0f;
                viewport.mMaxDepth = 1.0f;
                cmd.RHISetViewport(viewport);

                Rhi::FRhiScissorRect scissor{};
                scissor.mX      = viewRect.X;
                scissor.mY      = viewRect.Y;
                scissor.mWidth  = viewRect.Width;
                scissor.mHeight = viewRect.Height;
                cmd.RHISetScissor(scissor);

                cmd.RHISetBindGroup(0U, bindGroup.Get(), nullptr, 0U);
                cmd.RHISetPrimitiveTopology(Rhi::ERhiPrimitiveTopology::TriangleList);
                cmd.RHIDraw(3U, 1U, 0U, 0U);

                // After writing history, mark it valid and flip ping-pong for next frame.
                TemporalAA::Detail::CommitHistoryWritten(viewKey);
            });

        if (outRef.IsValid()) {
            io.SceneColor = outRef;
        }
    }
} // namespace AltinaEngine::Rendering::PostProcess::Builtin
