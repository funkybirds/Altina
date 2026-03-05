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
#include "Utility/Assert.h"
#include "View/ViewData.h"

#include <mutex>

namespace AltinaEngine::Rendering::PostProcess::Builtin {
    void AddTaa(RenderCore::FFrameGraph& graph, const RenderCore::View::FViewData& view,
        const FPostProcessNode& node, const FPostProcessBuildContext& ctx, FPostProcessIO& io);
    void AddBloom(RenderCore::FFrameGraph& graph, const RenderCore::View::FViewData& view,
        const FPostProcessNode& node, const FPostProcessBuildContext& ctx, FPostProcessIO& io);
    void AddTonemap(RenderCore::FFrameGraph& graph, const RenderCore::View::FViewData& view,
        const FPostProcessNode& node, const FPostProcessBuildContext& ctx, FPostProcessIO& io);
    void AddFxaa(RenderCore::FFrameGraph& graph, const RenderCore::View::FViewData& view,
        const FPostProcessNode& node, const FPostProcessBuildContext& ctx, FPostProcessIO& io);
} // namespace AltinaEngine::Rendering::PostProcess::Builtin

namespace AltinaEngine::Rendering {
    using Core::Utility::Assert;
    using Core::Utility::DebugAssert;

    namespace {
        struct FEffectEntry {
            FPostProcessAddToGraphFn Fn = nullptr;
        };

        struct FRegistryState {
            std::mutex                      Mutex;
            THashMap<FString, FEffectEntry> Effects;
            THashMap<FString, bool>         MissingLoggedOnce;
            bool                            bBuiltinsRegistered = false;
        };

        auto GetRegistryState() -> FRegistryState& {
            static FRegistryState s{};
            return s;
        }

        void EnsureBuiltinEffectsRegistered() {
            auto& s = GetRegistryState();
            if (s.bBuiltinsRegistered) {
                return;
            }

            std::scoped_lock lock(s.Mutex);
            if (s.bBuiltinsRegistered) {
                return;
            }

            // Built-ins.
            s.Effects[FString(TEXT("TAA"))]     = FEffectEntry{ &PostProcess::Builtin::AddTaa };
            s.Effects[FString(TEXT("Bloom"))]   = FEffectEntry{ &PostProcess::Builtin::AddBloom };
            s.Effects[FString(TEXT("Tonemap"))] = FEffectEntry{ &PostProcess::Builtin::AddTonemap };
            s.Effects[FString(TEXT("Fxaa"))]    = FEffectEntry{ &PostProcess::Builtin::AddFxaa };
            // Common alias.
            s.Effects[FString(TEXT("FXAA"))]  = FEffectEntry{ &PostProcess::Builtin::AddFxaa };
            s.Effects[FString(TEXT("BLOOM"))] = FEffectEntry{ &PostProcess::Builtin::AddBloom };

            s.bBuiltinsRegistered = true;
        }

        void AddPresent(RenderCore::FFrameGraph& graph, const RenderCore::View::FViewData& view,
            const FPostProcessBuildContext& ctx, const FPostProcessIO& io) {
            if (!ctx.BackBuffer.IsValid()) {
                DebugAssert(
                    false, TEXT("PostProcess"), "AddPresent skipped: backbuffer is invalid.");
                return;
            }

            if (!PostProcess::Detail::EnsurePostProcessSharedResources()) {
                DebugAssert(false, TEXT("PostProcess"),
                    "AddPresent skipped: shared post-process resources are not ready.");
                return;
            }

            struct FPassData {
                RenderCore::FFrameGraphTextureRef In;
                RenderCore::FFrameGraphTextureRef Out;
                RenderCore::FFrameGraphRTVRef     OutRTV;
                bool                              bHasInput = false;
            };

            RenderCore::FFrameGraphPassDesc desc{};
            desc.mName  = "PostProcess.Present";
            desc.mType  = RenderCore::EFrameGraphPassType::Raster;
            desc.mQueue = RenderCore::EFrameGraphQueue::Graphics;

            graph.AddPass<FPassData>(
                desc,
                [&](RenderCore::FFrameGraphPassBuilder& builder, FPassData& data) {
                    data.Out = builder.Write(ctx.BackBuffer, Rhi::ERhiResourceState::RenderTarget);
                    data.bHasInput = io.SceneColor.IsValid();
                    if (data.bHasInput) {
                        data.In =
                            builder.Read(io.SceneColor, Rhi::ERhiResourceState::ShaderResource);
                    }

                    Rhi::FRhiTextureViewRange viewRange{};
                    viewRange.mMipCount        = 1U;
                    viewRange.mLayerCount      = 1U;
                    viewRange.mDepthSliceCount = 1U;

                    Rhi::FRhiRenderTargetViewDesc rtvDesc{};
                    rtvDesc.mDebugName.Assign(TEXT("BackBuffer.RTV"));
                    rtvDesc.mFormat = (ctx.BackBufferFormat != Rhi::ERhiFormat::Unknown)
                        ? ctx.BackBufferFormat
                        : Rhi::ERhiFormat::B8G8R8A8Unorm;
                    rtvDesc.mRange  = viewRange;
                    data.OutRTV     = builder.CreateRTV(data.Out, rtvDesc);

                    RenderCore::FRdgRenderTargetBinding rtv{};
                    rtv.mRTV        = data.OutRTV;
                    rtv.mLoadOp     = Rhi::ERhiLoadOp::Clear;
                    rtv.mStoreOp    = Rhi::ERhiStoreOp::Store;
                    rtv.mClearColor = { 0.0f, 0.0f, 0.0f, 1.0f };
                    builder.SetRenderTargets(&rtv, 1U, nullptr);

                    builder.SetExternalOutput(data.Out, Rhi::ERhiResourceState::Present);
                },
                [viewRect = view.ViewRect](Rhi::FRhiCmdContext& cmd,
                    const RenderCore::FFrameGraphPassResources& res,
                    const FPassData&                            data) -> void {
                    if (!data.bHasInput) {
                        DebugAssert(false, TEXT("PostProcess"),
                            "AddPresent pass skipped draw: no SceneColor input.");
                        return;
                    }

                    auto& shared = PostProcess::Detail::GetPostProcessSharedResources();
                    if (!shared.BlitPipeline || !shared.Layout || !shared.LinearSampler
                        || !shared.BlitConstantsBuffer) {
                        DebugAssert(false, TEXT("PostProcess"),
                            "AddPresent pass skipped draw: missing shared Blit resources.");
                        return;
                    }

                    auto* inTex  = res.GetTexture(data.In);
                    auto* device = Rhi::RHIGetDevice();
                    if (!inTex || device == nullptr) {
                        DebugAssert(false, TEXT("PostProcess"),
                            "AddPresent pass skipped draw: input texture/device is null (inRef={}, outRef={}, hasInput={}, inTex={}, device={}).",
                            data.In.mId, data.Out.mId, data.bHasInput ? 1U : 0U,
                            static_cast<u32>(inTex != nullptr),
                            static_cast<u32>(device != nullptr));
                        return;
                    }

                    Rhi::FRhiBindGroupDesc groupDesc{};
                    if (!PostProcess::Detail::BuildCommonBindGroupDesc(shared,
                            PostProcess::Detail::kNameBlitConstants,
                            shared.BlitConstantsBuffer.Get(),
                            static_cast<u64>(sizeof(PostProcess::Detail::FBlitConstants)), inTex,
                            groupDesc)) {
                        DebugAssert(false, TEXT("PostProcess"),
                            "AddPresent pass skipped draw: failed to build blit bind group desc.");
                        return;
                    }

                    auto bindGroup = device->CreateBindGroup(groupDesc);
                    if (!bindGroup) {
                        Assert(false, TEXT("PostProcess"),
                            "AddPresent pass failed: CreateBindGroup returned null.");
                        return;
                    }

                    cmd.RHISetGraphicsPipeline(shared.BlitPipeline.Get());

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
        }
    } // namespace

    auto RegisterPostProcessEffect(FStringView effectId, FPostProcessAddToGraphFn fn) noexcept
        -> bool {
        if (effectId.IsEmpty() || fn == nullptr) {
            return false;
        }

        auto&            s = GetRegistryState();
        std::scoped_lock lock(s.Mutex);
        s.Effects[FString(effectId)] = FEffectEntry{ fn };
        return true;
    }

    auto UnregisterPostProcessEffect(FStringView effectId) noexcept -> bool {
        if (effectId.IsEmpty()) {
            return false;
        }
        auto&            s = GetRegistryState();
        std::scoped_lock lock(s.Mutex);
        const auto       it = s.Effects.find(FString(effectId));
        if (it == s.Effects.end()) {
            return false;
        }
        s.Effects.erase(it);
        return true;
    }

    void BuildPostProcess(RenderCore::FFrameGraph& graph, const RenderCore::View::FViewData& view,
        const FPostProcessStack& stack, FPostProcessIO& io, const FPostProcessBuildContext& ctx) {
        EnsureBuiltinEffectsRegistered();

        const bool bEnable = stack.bEnable && io.SceneColor.IsValid();

        if (bEnable) {
            auto& s = GetRegistryState();

            for (const auto& node : stack.Stack) {
                if (!node.bEnabled) {
                    continue;
                }
                if (node.EffectId.IsEmptyString()) {
                    continue;
                }

                FPostProcessAddToGraphFn fn = nullptr;
                {
                    std::scoped_lock lock(s.Mutex);
                    const auto       it = s.Effects.find(node.EffectId);
                    if (it != s.Effects.end()) {
                        fn = it->second.Fn;
                    } else if (!s.MissingLoggedOnce.HasKey(node.EffectId)) {
                        s.MissingLoggedOnce[node.EffectId] = true;
                        LogWarning(TEXT("PostProcess effect '{}' is not registered. Skipping."),
                            node.EffectId.ToView());
                    }
                }

                if (fn != nullptr) {
                    fn(graph, view, node, ctx, io);
                }
            }
        }

        // Always add the final Present pass.
        AddPresent(graph, view, ctx, io);
    }

    void ShutdownPostProcess() noexcept {
        auto&            s = GetRegistryState();
        std::scoped_lock lock(s.Mutex);
        s.Effects.clear();
        s.MissingLoggedOnce.clear();
        s.bBuiltinsRegistered = false;

        PostProcess::Detail::ShutdownPostProcessSharedResources();
    }
} // namespace AltinaEngine::Rendering
