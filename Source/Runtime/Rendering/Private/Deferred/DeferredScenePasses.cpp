#include "Deferred/DeferredScenePasses.h"

#include "Logging/Log.h"
#include "Utility/Assert.h"
#include "Rhi/Command/RhiCmdContext.h"
#include "Rhi/RhiBindGroup.h"
#include "Rhi/RhiDevice.h"
#include "Rhi/RhiInit.h"

namespace AltinaEngine::Rendering::Deferred {
    namespace {
        using Core::Container::FStringView;
        using Core::Utility::Assert;
        using Core::Utility::DebugAssert;

        constexpr auto     kNameDeferredView  = TEXT("DeferredView");
        constexpr auto     kNameIblConstants  = TEXT("IblConstants");
        constexpr auto     kNameGBufferA      = TEXT("GBufferA");
        constexpr auto     kNameGBufferB      = TEXT("GBufferB");
        constexpr auto     kNameGBufferC      = TEXT("GBufferC");
        constexpr auto     kNameSceneDepth    = TEXT("SceneDepth");
        constexpr auto     kNameShadowMap     = TEXT("ShadowMap");
        constexpr auto     kNameSkyIrrCube    = TEXT("SkyIrradianceCube");
        constexpr auto     kNameSkySpecCube   = TEXT("SkySpecularCube");
        constexpr auto     kNameBrdfLut       = TEXT("BrdfLut");
        constexpr auto     kNameSsaoTex       = TEXT("SsaoTex");
        constexpr auto     kNameLinearSampler = TEXT("LinearSampler");
        constexpr auto     kNameSkyCube       = TEXT("SkyCube");

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

    void AddDeferredLightingPass(
        FDeferredLightingPassInputs& inputs, RenderCore::FFrameGraphTextureRef& outSceneColorHDR) {
        DebugAssert(inputs.Graph != nullptr, TEXT("BasicDeferredRenderer"),
            "DeferredLighting: frame graph input is null.");
        DebugAssert(inputs.ViewRect != nullptr, TEXT("BasicDeferredRenderer"),
            "DeferredLighting: view rect input is null.");
        DebugAssert(inputs.PerFrameConstants != nullptr, TEXT("BasicDeferredRenderer"),
            "DeferredLighting: per-frame constants input is null.");
        if (inputs.Graph == nullptr || inputs.ViewRect == nullptr
            || inputs.PerFrameConstants == nullptr) {
            return;
        }

        auto& graph = *inputs.Graph;

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
                auto registerExternalTextureRead = [&](Rhi::FRhiTexture* texture) -> void {
                    if (texture == nullptr) {
                        return;
                    }
                    const auto ref = graph.ImportTexture(texture, Rhi::ERhiResourceState::Common);
                    if (ref.IsValid()) {
                        builder.Read(ref, Rhi::ERhiResourceState::ShaderResource);
                    }
                };

                if (inputs.GBufferA.IsValid()) {
                    builder.Read(inputs.GBufferA, Rhi::ERhiResourceState::ShaderResource);
                }
                if (inputs.GBufferB.IsValid()) {
                    builder.Read(inputs.GBufferB, Rhi::ERhiResourceState::ShaderResource);
                }
                if (inputs.GBufferC.IsValid()) {
                    builder.Read(inputs.GBufferC, Rhi::ERhiResourceState::ShaderResource);
                }
                if (inputs.SceneDepth.IsValid()) {
                    builder.Read(inputs.SceneDepth, Rhi::ERhiResourceState::ShaderResource);
                }
                if (inputs.SsaoTexture.IsValid()) {
                    builder.Read(inputs.SsaoTexture, Rhi::ERhiResourceState::ShaderResource);
                }

                registerExternalTextureRead(inputs.SkyIrradiance);
                registerExternalTextureRead(inputs.SkySpecular);
                registerExternalTextureRead(inputs.BrdfLut);
                registerExternalTextureRead(inputs.IblBlackCube);
                registerExternalTextureRead(inputs.IblBlack2D);

                if (inputs.ShadowMap.IsValid()) {
                    builder.Read(inputs.ShadowMap, Rhi::ERhiResourceState::ShaderResource);
                    data.Shadow = inputs.ShadowMap;
                } else {
                    RenderCore::FFrameGraphTextureDesc shadowDesc{};
                    shadowDesc.mDesc.mDebugName.Assign(TEXT("ShadowMap.Dummy"));
                    shadowDesc.mDesc.mWidth       = 1U;
                    shadowDesc.mDesc.mHeight      = 1U;
                    shadowDesc.mDesc.mArrayLayers = 1U;
                    shadowDesc.mDesc.mFormat      = Rhi::ERhiFormat::D32Float;
                    shadowDesc.mDesc.mBindFlags   = Rhi::ERhiTextureBindFlags::DepthStencil
                        | Rhi::ERhiTextureBindFlags::ShaderResource;
                    data.Shadow = builder.CreateTexture(shadowDesc);
                    builder.Read(data.Shadow, Rhi::ERhiResourceState::ShaderResource);
                }

                data.GBufferA = inputs.GBufferA;
                data.GBufferB = inputs.GBufferB;
                data.GBufferC = inputs.GBufferC;
                data.Depth    = inputs.SceneDepth;
                data.Ssao     = inputs.SsaoTexture;

                RenderCore::FFrameGraphTextureDesc hdrDesc{};
                hdrDesc.mDesc.mDebugName.Assign(TEXT("SceneColorHDR"));
                hdrDesc.mDesc.mWidth       = inputs.Width;
                hdrDesc.mDesc.mHeight      = inputs.Height;
                hdrDesc.mDesc.mArrayLayers = 1U;
                hdrDesc.mDesc.mFormat      = Rhi::ERhiFormat::R16G16B16A16Float;
                hdrDesc.mDesc.mBindFlags   = Rhi::ERhiTextureBindFlags::RenderTarget
                    | Rhi::ERhiTextureBindFlags::ShaderResource;
                outSceneColorHDR = builder.CreateTexture(hdrDesc);

                data.Output = builder.Write(outSceneColorHDR, Rhi::ERhiResourceState::RenderTarget);

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
            [viewRect = *inputs.ViewRect, sharedConstants = *inputs.PerFrameConstants,
                perFrameBuffer = inputs.PerFrameBuffer, iblBuffer = inputs.IblConstantsBuffer,
                pipeline = inputs.Pipeline, layout = inputs.Layout, sampler = inputs.Sampler,
                bindings = inputs.Bindings, skyIrradiance = inputs.SkyIrradiance,
                skySpecular = inputs.SkySpecular, brdfLut = inputs.BrdfLut,
                iblBlackCube = inputs.IblBlackCube, iblBlack2D = inputs.IblBlack2D,
                settings = inputs.RuntimeSettings](Rhi::FRhiCmdContext& ctx,
                const RenderCore::FFrameGraphPassResources&             res,
                const FLightingPassData&                                data) -> void {
                if (!pipeline || !layout || !sampler || !bindings) {
                    DebugAssert(false, TEXT("BasicDeferredRenderer"),
                        "DeferredLighting skipped: shared pipeline/layout/sampler/bindings missing.");
                    return;
                }

                auto* texA      = res.GetTexture(data.GBufferA);
                auto* texB      = res.GetTexture(data.GBufferB);
                auto* texC      = res.GetTexture(data.GBufferC);
                auto* depthTex  = res.GetTexture(data.Depth);
                auto* shadowTex = res.GetTexture(data.Shadow);
                auto* ssaoTex   = res.GetTexture(data.Ssao);
                if (!texA || !texB || !texC || !depthTex || !shadowTex || !ssaoTex) {
                    return;
                }

                auto* device = Rhi::RHIGetDevice();
                if (!device || perFrameBuffer == nullptr) {
                    return;
                }

                ctx.RHIUpdateDynamicBufferDiscard(
                    perFrameBuffer, &sharedConstants, sizeof(sharedConstants), 0ULL);

                FIblConstants iblConstants{};
                if (settings.bEnableIbl) {
                    iblConstants.EnvDiffuseIntensity  = settings.IblDiffuseIntensity;
                    iblConstants.EnvSpecularIntensity = settings.IblSpecularIntensity;
                    iblConstants.SpecularMaxLod       = settings.SpecularMaxLod;
                    iblConstants.EnvSaturation        = settings.IblSaturation;
                }
                if (iblBuffer != nullptr) {
                    ctx.RHIUpdateDynamicBufferDiscard(
                        iblBuffer, &iblConstants, sizeof(iblConstants), 0ULL);
                }

                auto*     irrTex  = settings.bEnableIbl ? skyIrradiance : iblBlackCube;
                auto*     specTex = settings.bEnableIbl ? skySpecular : iblBlackCube;
                auto*     lutTex  = settings.bEnableIbl ? brdfLut : iblBlack2D;

                const u32 lightingPerFrameBinding = RequireBinding(*bindings, kNameDeferredView,
                    Rhi::ERhiBindingType::ConstantBuffer, TEXT("Lighting"));
                const u32 lightingIblBinding      = RequireBinding(*bindings, kNameIblConstants,
                         Rhi::ERhiBindingType::ConstantBuffer, TEXT("Lighting"));
                const u32 lightingGBufferABinding = RequireBinding(*bindings, kNameGBufferA,
                    Rhi::ERhiBindingType::SampledTexture, TEXT("Lighting"));
                const u32 lightingGBufferBBinding = RequireBinding(*bindings, kNameGBufferB,
                    Rhi::ERhiBindingType::SampledTexture, TEXT("Lighting"));
                const u32 lightingGBufferCBinding = RequireBinding(*bindings, kNameGBufferC,
                    Rhi::ERhiBindingType::SampledTexture, TEXT("Lighting"));
                const u32 lightingDepthBinding    = RequireBinding(*bindings, kNameSceneDepth,
                       Rhi::ERhiBindingType::SampledTexture, TEXT("Lighting"));
                const u32 lightingShadowBinding   = RequireBinding(*bindings, kNameShadowMap,
                      Rhi::ERhiBindingType::SampledTexture, TEXT("Lighting"));
                const u32 lightingIrrBinding      = RequireBinding(*bindings, kNameSkyIrrCube,
                         Rhi::ERhiBindingType::SampledTexture, TEXT("Lighting"));
                const u32 lightingSpecBinding     = RequireBinding(*bindings, kNameSkySpecCube,
                        Rhi::ERhiBindingType::SampledTexture, TEXT("Lighting"));
                const u32 lightingBrdfBinding     = RequireBinding(*bindings, kNameBrdfLut,
                        Rhi::ERhiBindingType::SampledTexture, TEXT("Lighting"));
                const u32 lightingSsaoBinding     = RequireBinding(*bindings, kNameSsaoTex,
                        Rhi::ERhiBindingType::SampledTexture, TEXT("Lighting"));
                const u32 lightingSamplerBinding  = RequireBinding(
                    *bindings, kNameLinearSampler, Rhi::ERhiBindingType::Sampler, TEXT("Lighting"));

                RenderCore::ShaderBinding::FBindGroupBuilder builder(layout);
                DebugAssert(builder.AddBuffer(lightingPerFrameBinding, perFrameBuffer, 0ULL,
                                static_cast<u64>(sizeof(FPerFrameConstants))),
                    TEXT("BasicDeferredRenderer"), "Lighting bind group: add per-frame failed.");
                DebugAssert(builder.AddBuffer(lightingIblBinding,
                                (iblBuffer != nullptr) ? iblBuffer : perFrameBuffer, 0ULL,
                                static_cast<u64>(sizeof(FIblConstants))),
                    TEXT("BasicDeferredRenderer"), "Lighting bind group: add IBL failed.");
                DebugAssert(builder.AddTexture(lightingGBufferABinding, texA),
                    TEXT("BasicDeferredRenderer"), "Lighting bind group: add GBufferA failed.");
                DebugAssert(builder.AddTexture(lightingGBufferBBinding, texB),
                    TEXT("BasicDeferredRenderer"), "Lighting bind group: add GBufferB failed.");
                DebugAssert(builder.AddTexture(lightingGBufferCBinding, texC),
                    TEXT("BasicDeferredRenderer"), "Lighting bind group: add GBufferC failed.");
                DebugAssert(builder.AddTexture(lightingDepthBinding, depthTex),
                    TEXT("BasicDeferredRenderer"), "Lighting bind group: add Depth failed.");
                DebugAssert(builder.AddTexture(lightingShadowBinding, shadowTex),
                    TEXT("BasicDeferredRenderer"), "Lighting bind group: add Shadow failed.");
                DebugAssert(builder.AddTexture(lightingIrrBinding, irrTex),
                    TEXT("BasicDeferredRenderer"), "Lighting bind group: add Irr failed.");
                DebugAssert(builder.AddTexture(lightingSpecBinding, specTex),
                    TEXT("BasicDeferredRenderer"), "Lighting bind group: add Spec failed.");
                DebugAssert(builder.AddTexture(lightingBrdfBinding, lutTex),
                    TEXT("BasicDeferredRenderer"), "Lighting bind group: add Brdf failed.");
                DebugAssert(builder.AddTexture(lightingSsaoBinding, ssaoTex),
                    TEXT("BasicDeferredRenderer"), "Lighting bind group: add Ssao failed.");
                DebugAssert(builder.AddSampler(lightingSamplerBinding, sampler),
                    TEXT("BasicDeferredRenderer"), "Lighting bind group: add sampler failed.");

                Rhi::FRhiBindGroupDesc groupDesc{};
                DebugAssert(builder.Build(groupDesc), TEXT("BasicDeferredRenderer"),
                    "Lighting bind group: builder/layout mismatch.");
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

    void AddDeferredSkyBoxPass(FDeferredSkyBoxPassInputs& inputs) {
        DebugAssert(inputs.Graph != nullptr, TEXT("BasicDeferredRenderer"),
            "SkyBox: frame graph input is null.");
        DebugAssert(inputs.ViewRect != nullptr, TEXT("BasicDeferredRenderer"),
            "SkyBox: view rect input is null.");
        if (inputs.Graph == nullptr || inputs.ViewRect == nullptr || !inputs.SkyCube
            || !inputs.Pipeline || !inputs.Layout || !inputs.Sampler || !inputs.Bindings
            || !inputs.PerFrameBuffer || !inputs.SceneDepth.IsValid()
            || !inputs.SceneColorHDR.IsValid()) {
            return;
        }

        auto& graph = *inputs.Graph;

        struct FSkyBoxPassData {
            RenderCore::FFrameGraphTextureRef Depth;
            RenderCore::FFrameGraphTextureRef Output;
            RenderCore::FFrameGraphRTVRef     OutputRTV;
        };

        RenderCore::FFrameGraphPassDesc passDesc{};
        passDesc.mName  = "BasicDeferred.SkyBox";
        passDesc.mType  = RenderCore::EFrameGraphPassType::Raster;
        passDesc.mQueue = RenderCore::EFrameGraphQueue::Graphics;

        graph.AddPass<FSkyBoxPassData>(
            passDesc,
            [&](RenderCore::FFrameGraphPassBuilder& builder, FSkyBoxPassData& data) {
                builder.Read(inputs.SceneDepth, Rhi::ERhiResourceState::ShaderResource);
                data.Depth =
                    builder.Read(inputs.SceneDepth, Rhi::ERhiResourceState::ShaderResource);
                if (inputs.SkyCube != nullptr) {
                    const auto skyCubeRef =
                        graph.ImportTexture(inputs.SkyCube, Rhi::ERhiResourceState::Common);
                    if (skyCubeRef.IsValid()) {
                        builder.Read(skyCubeRef, Rhi::ERhiResourceState::ShaderResource);
                    }
                }
                data.Output =
                    builder.Write(inputs.SceneColorHDR, Rhi::ERhiResourceState::RenderTarget);

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
            [viewRect = *inputs.ViewRect, pipeline = inputs.Pipeline, layout = inputs.Layout,
                sampler = inputs.Sampler, bindings = inputs.Bindings,
                perFrameBuffer = inputs.PerFrameBuffer, skyCube = inputs.SkyCube](
                Rhi::FRhiCmdContext& ctx, const RenderCore::FFrameGraphPassResources& res,
                const FSkyBoxPassData& data) -> void {
                auto* depthTex = res.GetTexture(data.Depth);
                auto* device   = Rhi::RHIGetDevice();
                if (!depthTex || !device) {
                    return;
                }

                const u32 skyPerFrameBinding = RequireBinding(*bindings, kNameDeferredView,
                    Rhi::ERhiBindingType::ConstantBuffer, TEXT("SkyBox"));
                const u32 skyDepthBinding    = RequireBinding(*bindings, kNameSceneDepth,
                       Rhi::ERhiBindingType::SampledTexture, TEXT("SkyBox"));
                const u32 skyCubeBinding     = RequireBinding(
                    *bindings, kNameSkyCube, Rhi::ERhiBindingType::SampledTexture, TEXT("SkyBox"));
                const u32 skySamplerBinding = RequireBinding(
                    *bindings, kNameLinearSampler, Rhi::ERhiBindingType::Sampler, TEXT("SkyBox"));

                RenderCore::ShaderBinding::FBindGroupBuilder builder(layout);
                DebugAssert(builder.AddBuffer(skyPerFrameBinding, perFrameBuffer, 0ULL,
                                static_cast<u64>(sizeof(FPerFrameConstants))),
                    TEXT("BasicDeferredRenderer"), "SkyBox bind group: add per-frame failed.");
                DebugAssert(builder.AddTexture(skyDepthBinding, depthTex),
                    TEXT("BasicDeferredRenderer"), "SkyBox bind group: add depth failed.");
                DebugAssert(builder.AddTexture(skyCubeBinding, skyCube),
                    TEXT("BasicDeferredRenderer"), "SkyBox bind group: add skycube failed.");
                DebugAssert(builder.AddSampler(skySamplerBinding, sampler),
                    TEXT("BasicDeferredRenderer"), "SkyBox bind group: add sampler failed.");

                Rhi::FRhiBindGroupDesc groupDesc{};
                DebugAssert(builder.Build(groupDesc), TEXT("BasicDeferredRenderer"),
                    "SkyBox bind group: builder/layout mismatch.");
                auto bindGroup = device->CreateBindGroup(groupDesc);
                if (!bindGroup) {
                    return;
                }

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
