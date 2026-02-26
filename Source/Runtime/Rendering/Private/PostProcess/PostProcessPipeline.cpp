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
                return;
            }

            if (!PostProcess::Detail::EnsurePostProcessSharedResources()) {
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
                        return;
                    }

                    auto& shared = PostProcess::Detail::GetPostProcessSharedResources();
                    if (!shared.BlitPipeline || !shared.Layout || !shared.LinearSampler
                        || !shared.BlitConstantsBuffer) {
                        return;
                    }

                    auto* inTex  = res.GetTexture(data.In);
                    auto* device = Rhi::RHIGetDevice();
                    if (!inTex || device == nullptr) {
                        return;
                    }

                    // Bind group: constants + input texture + sampler.
                    Rhi::FRhiBindGroupDesc groupDesc{};
                    groupDesc.mLayout = shared.Layout.Get();

                    Rhi::FRhiBindGroupEntry cb{};
                    cb.mBinding = 0U;
                    cb.mType    = Rhi::ERhiBindingType::ConstantBuffer;
                    cb.mBuffer  = shared.BlitConstantsBuffer.Get();
                    cb.mOffset  = 0ULL;
                    cb.mSize    = static_cast<u64>(sizeof(PostProcess::Detail::FBlitConstants));
                    groupDesc.mEntries.PushBack(cb);

                    Rhi::FRhiBindGroupEntry tex{};
                    tex.mBinding = 0U;
                    tex.mType    = Rhi::ERhiBindingType::SampledTexture;
                    tex.mTexture = inTex;
                    groupDesc.mEntries.PushBack(tex);

                    Rhi::FRhiBindGroupEntry sampler{};
                    sampler.mBinding = 0U;
                    sampler.mType    = Rhi::ERhiBindingType::Sampler;
                    sampler.mSampler = shared.LinearSampler.Get();
                    groupDesc.mEntries.PushBack(sampler);

                    auto bindGroup = device->CreateBindGroup(groupDesc);
                    if (!bindGroup) {
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
} // namespace AltinaEngine::Rendering
