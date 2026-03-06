#include "Deferred/DeferredSsaoPass.h"

#include "Container/StringView.h"
#include "Rhi/Command/RhiCmdContext.h"
#include "Rhi/RhiBindGroup.h"
#include "Rhi/RhiDebugMarker.h"
#include "Rhi/RhiDevice.h"
#include "Rhi/RhiInit.h"
#include "Utility/Assert.h"

namespace AltinaEngine::Rendering::Deferred {
    namespace {
        using Core::Container::FStringView;
        using Core::Utility::Assert;
        using Core::Utility::DebugAssert;

        constexpr auto kNameDeferredView  = TEXT("DeferredView");
        constexpr auto kNameSsaoConstants = TEXT("SsaoConstants");
        constexpr auto kNameGBufferB      = TEXT("GBufferB");
        constexpr auto kNameSceneDepth    = TEXT("SceneDepth");
        constexpr auto kNameLinearSampler = TEXT("LinearSampler");

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

        [[nodiscard]] auto RequireBinding(
            const RenderCore::ShaderBinding::FBindingLookupTable& table, const TChar* name,
            Rhi::ERhiBindingType type, const TChar* passTag) -> u32 {
            const u32 nameHash = RenderCore::ShaderBinding::HashBindingName(FStringView(name));
            u32       binding  = RenderCore::ShaderBinding::kInvalidBinding;
            DebugAssert(
                RenderCore::ShaderBinding::FindBindingByNameHash(table, nameHash, type, binding),
                TEXT("BasicDeferredRenderer"),
                "{}: failed to resolve binding from lookup table (name={}, hash={}, type={}).",
                passTag, name, nameHash, static_cast<u32>(type));
            return binding;
        }
    } // namespace

    void AddDeferredSsaoPass(
        FDeferredSsaoPassInputs& inputs, RenderCore::FFrameGraphTextureRef& outSsaoTexture) {
        DebugAssert(inputs.Graph != nullptr, TEXT("BasicDeferredRenderer"),
            "SSAO: frame graph input is null.");
        DebugAssert(inputs.ViewRect != nullptr, TEXT("BasicDeferredRenderer"),
            "SSAO: view rect input is null.");
        DebugAssert(inputs.PerFrameConstants != nullptr, TEXT("BasicDeferredRenderer"),
            "SSAO: per-frame constants input is null.");
        if (inputs.Graph == nullptr || inputs.ViewRect == nullptr
            || inputs.PerFrameConstants == nullptr || !inputs.GBufferB.IsValid()
            || !inputs.SceneDepth.IsValid()) {
            return;
        }

        auto& graph = *inputs.Graph;

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
                data.GBufferB =
                    builder.Read(inputs.GBufferB, Rhi::ERhiResourceState::ShaderResource);
                data.Depth =
                    builder.Read(inputs.SceneDepth, Rhi::ERhiResourceState::ShaderResource);

                RenderCore::FFrameGraphTextureDesc aoDesc{};
                aoDesc.mDesc.mDebugName.Assign(TEXT("SSAO"));
                aoDesc.mDesc.mWidth     = inputs.Width;
                aoDesc.mDesc.mHeight    = inputs.Height;
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

                outSsaoTexture = data.Output;
            },
            [viewRect = *inputs.ViewRect, sharedConstants = *inputs.PerFrameConstants,
                pipeline = inputs.Pipeline, layout = inputs.Layout, sampler = inputs.Sampler,
                bindings = inputs.Bindings, perFrameBuffer = inputs.PerFrameBuffer,
                ssaoBuffer = inputs.SsaoConstantsBuffer, runtime = inputs.RuntimeSettings](
                Rhi::FRhiCmdContext& ctx, const RenderCore::FFrameGraphPassResources& res,
                const FSsaoPassData& data) -> void {
                Rhi::FRhiDebugMarker marker(ctx, TEXT("Deferred.SSAO"));
                if (!pipeline || !layout || !sampler || !bindings || !perFrameBuffer
                    || !ssaoBuffer) {
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

                ctx.RHIUpdateDynamicBufferDiscard(
                    perFrameBuffer, &sharedConstants, sizeof(sharedConstants), 0ULL);

                FSsaoConstants ssao{};
                ssao.Enable      = runtime.Enable;
                ssao.SampleCount = runtime.SampleCount;
                ssao.RadiusVS    = runtime.RadiusVS;
                ssao.BiasNdc     = runtime.BiasNdc;
                ssao.Power       = runtime.Power;
                ssao.Intensity   = runtime.Intensity;
                ctx.RHIUpdateDynamicBufferDiscard(ssaoBuffer, &ssao, sizeof(ssao), 0ULL);

                const u32 ssaoPerFrameBinding  = RequireBinding(*bindings, kNameDeferredView,
                     Rhi::ERhiBindingType::ConstantBuffer, TEXT("SSAO"));
                const u32 ssaoConstantsBinding = RequireBinding(*bindings, kNameSsaoConstants,
                    Rhi::ERhiBindingType::ConstantBuffer, TEXT("SSAO"));
                const u32 ssaoNormalBinding    = RequireBinding(
                    *bindings, kNameGBufferB, Rhi::ERhiBindingType::SampledTexture, TEXT("SSAO"));
                const u32 ssaoDepthBinding = RequireBinding(
                    *bindings, kNameSceneDepth, Rhi::ERhiBindingType::SampledTexture, TEXT("SSAO"));
                const u32 ssaoSamplerBinding = RequireBinding(
                    *bindings, kNameLinearSampler, Rhi::ERhiBindingType::Sampler, TEXT("SSAO"));

                RenderCore::ShaderBinding::FBindGroupBuilder builder(layout);
                DebugAssert(builder.AddBuffer(ssaoPerFrameBinding, perFrameBuffer, 0ULL,
                                static_cast<u64>(sizeof(FPerFrameConstants))),
                    TEXT("BasicDeferredRenderer"),
                    "SSAO bind group: failed to add per-frame cbuffer (binding={}).",
                    ssaoPerFrameBinding);
                DebugAssert(builder.AddBuffer(ssaoConstantsBinding, ssaoBuffer, 0ULL,
                                static_cast<u64>(sizeof(FSsaoConstants))),
                    TEXT("BasicDeferredRenderer"),
                    "SSAO bind group: failed to add constants cbuffer (binding={}).",
                    ssaoConstantsBinding);
                DebugAssert(builder.AddTexture(ssaoNormalBinding, normalTex),
                    TEXT("BasicDeferredRenderer"),
                    "SSAO bind group: failed to add normal texture (binding={}).",
                    ssaoNormalBinding);
                DebugAssert(builder.AddTexture(ssaoDepthBinding, depthTex),
                    TEXT("BasicDeferredRenderer"),
                    "SSAO bind group: failed to add depth texture (binding={}).", ssaoDepthBinding);
                DebugAssert(builder.AddSampler(ssaoSamplerBinding, sampler),
                    TEXT("BasicDeferredRenderer"),
                    "SSAO bind group: failed to add sampler (binding={}).", ssaoSamplerBinding);

                Rhi::FRhiBindGroupDesc groupDesc{};
                DebugAssert(builder.Build(groupDesc), TEXT("BasicDeferredRenderer"),
                    "SSAO bind group: builder/layout mismatch.");
                auto bindGroup = device->CreateBindGroup(groupDesc);
                Assert(!(!bindGroup), TEXT("BasicDeferredRenderer"), "Failed to create bind group");

                ctx.RHISetGraphicsPipeline(pipeline);

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
} // namespace AltinaEngine::Rendering::Deferred
