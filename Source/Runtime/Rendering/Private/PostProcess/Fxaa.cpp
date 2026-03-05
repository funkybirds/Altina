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
        using Detail::FFxaaConstants;

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

    void AddFxaa(RenderCore::FFrameGraph& graph, const RenderCore::View::FViewData& view,
        const FPostProcessNode& node, const FPostProcessBuildContext& ctx, FPostProcessIO& io) {
        if (!io.SceneColor.IsValid()) {
            return;
        }
        if (!ctx.BackBuffer.IsValid()) {
            return;
        }

        if (!Detail::EnsurePostProcessSharedResources()) {
            static bool sLoggedOnce = false;
            if (!sLoggedOnce) {
                sLoggedOnce = true;
                LogWarning(TEXT(
                    "PostProcess.Fxaa skipped: shared resources not ready (shader/pipeline creation failed?)"));
            }
            return;
        }

        const auto outputFormat = (ctx.BackBufferFormat != Rhi::ERhiFormat::Unknown)
            ? ctx.BackBufferFormat
            : Rhi::ERhiFormat::B8G8R8A8Unorm;

        RenderCore::FFrameGraphTextureDesc outDesc{};
        outDesc.mDesc.mDebugName.Assign(TEXT("PostProcess.SceneColorFXAA"));
        outDesc.mDesc.mWidth       = view.RenderTargetExtent.Width;
        outDesc.mDesc.mHeight      = view.RenderTargetExtent.Height;
        outDesc.mDesc.mArrayLayers = 1U;
        outDesc.mDesc.mFormat      = outputFormat;
        outDesc.mDesc.mBindFlags =
            Rhi::ERhiTextureBindFlags::RenderTarget | Rhi::ERhiTextureBindFlags::ShaderResource;

        const f32 edgeThreshold    = GetParamF32(node.Params, TEXT("EdgeThreshold"), 0.125f);
        const f32 edgeThresholdMin = GetParamF32(node.Params, TEXT("EdgeThresholdMin"), 0.0312f);
        const f32 subpix           = GetParamF32(node.Params, TEXT("Subpix"), 0.75f);

        RenderCore::FFrameGraphTextureRef outRef{};

        struct FPassData {
            RenderCore::FFrameGraphTextureRef In;
            RenderCore::FFrameGraphTextureRef Out;
            RenderCore::FFrameGraphRTVRef     OutRTV;
            f32                               EdgeThreshold    = 0.125f;
            f32                               EdgeThresholdMin = 0.0312f;
            f32                               Subpix           = 0.75f;
        };

        RenderCore::FFrameGraphPassDesc desc{};
        desc.mName  = "PostProcess.Fxaa";
        desc.mType  = RenderCore::EFrameGraphPassType::Raster;
        desc.mQueue = RenderCore::EFrameGraphQueue::Graphics;

        graph.AddPass<FPassData>(
            desc,
            [&](RenderCore::FFrameGraphPassBuilder& builder, FPassData& data) {
                data.EdgeThreshold    = edgeThreshold;
                data.EdgeThresholdMin = edgeThresholdMin;
                data.Subpix           = subpix;

                data.In  = builder.Read(io.SceneColor, Rhi::ERhiResourceState::ShaderResource);
                data.Out = builder.CreateTexture(outDesc);
                data.Out = builder.Write(data.Out, Rhi::ERhiResourceState::RenderTarget);
                outRef   = data.Out;

                Rhi::FRhiTextureViewRange viewRange{};
                viewRange.mMipCount        = 1U;
                viewRange.mLayerCount      = 1U;
                viewRange.mDepthSliceCount = 1U;

                Rhi::FRhiRenderTargetViewDesc rtvDesc{};
                rtvDesc.mDebugName.Assign(TEXT("PostProcess.SceneColorFXAA.RTV"));
                rtvDesc.mFormat = outputFormat;
                rtvDesc.mRange  = viewRange;
                data.OutRTV     = builder.CreateRTV(data.Out, rtvDesc);

                RenderCore::FRdgRenderTargetBinding rtv{};
                rtv.mRTV = data.OutRTV;
                builder.SetRenderTargets(&rtv, 1U, nullptr);
            },
            [viewRect = view.ViewRect](Rhi::FRhiCmdContext& cmd,
                const RenderCore::FFrameGraphPassResources& res, const FPassData& data) {
                auto& shared = Detail::GetPostProcessSharedResources();
                if (!shared.FxaaPipeline || !shared.Layout || !shared.LinearSampler
                    || !shared.FxaaConstantsBuffer) {
                    return;
                }

                auto* inTex  = res.GetTexture(data.In);
                auto* device = Rhi::RHIGetDevice();
                if (!inTex || device == nullptr) {
                    return;
                }

                FFxaaConstants constants{};
                constants.EdgeThreshold    = data.EdgeThreshold;
                constants.EdgeThresholdMin = data.EdgeThresholdMin;
                constants.Subpix           = data.Subpix;
                UpdateConstantBuffer(
                    shared.FxaaConstantsBuffer.Get(), &constants, sizeof(constants));

                Rhi::FRhiBindGroupDesc groupDesc{};
                if (!Detail::BuildCommonBindGroupDesc(shared, Detail::kNameFxaaConstants,
                        shared.FxaaConstantsBuffer.Get(), static_cast<u64>(sizeof(FFxaaConstants)),
                        inTex, groupDesc)) {
                    return;
                }

                auto bindGroup = device->CreateBindGroup(groupDesc);
                if (!bindGroup) {
                    return;
                }

                cmd.RHISetGraphicsPipeline(shared.FxaaPipeline.Get());

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
            });

        if (outRef.IsValid()) {
            io.SceneColor = outRef;
        }
    }
} // namespace AltinaEngine::Rendering::PostProcess::Builtin
