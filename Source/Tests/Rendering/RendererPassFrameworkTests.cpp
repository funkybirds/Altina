#include "TestHarness.h"

#include "Rendering/GraphicsPipelinePreset.h"
#include "Rendering/BasicDeferredRenderer.h"
#include "Rendering/Renderer.h"

namespace {
    using AltinaEngine::TChar;
    using AltinaEngine::Core::Container::FString;
    using AltinaEngine::Core::Container::TVector;
    using AltinaEngine::RenderCore::FMaterialPassState;
    using AltinaEngine::Rendering::BuildGraphicsPipelineDesc;
    using AltinaEngine::Rendering::ERendererGraphicsPipelinePreset;
    using AltinaEngine::Rendering::ERendererPassAnchorOrder;
    using AltinaEngine::Rendering::ERendererPassSet;
    using AltinaEngine::Rendering::FBaseRenderer;
    using AltinaEngine::Rendering::FBasicDeferredRenderer;
    using AltinaEngine::Rendering::FRendererBuilder;
    using AltinaEngine::Rendering::FRendererGraphicsPipelineBuildInputs;
    using AltinaEngine::Rendering::FRendererPassRegistration;
    using AltinaEngine::Rendering::IRendererPassProvider;

    auto ContainsId(const TVector<FString>& passIds, const TChar* id) -> bool {
        const FString targetId(id);
        for (const auto& passId : passIds) {
            if (passId == targetId) {
                return true;
            }
        }
        return false;
    }

    struct FTestRenderer final : public FBaseRenderer {
    public:
        struct FPassSpec {
            const TChar*             Id         = nullptr;
            ERendererPassSet         PassSet    = ERendererPassSet::DeferredBase;
            ERendererPassAnchorOrder AnchorMode = ERendererPassAnchorOrder::None;
            const TChar*             AnchorId   = nullptr;
        };

        explicit FTestRenderer(const TVector<FPassSpec>& inSpecs) : mSpecs(inSpecs) {}

        void PrepareForRendering(AltinaEngine::Rhi::FRhiDevice& device) override { (void)device; }
        void FinalizeRendering() override {}

    protected:
        void RegisterBuiltinPasses() override {
            for (const auto& spec : mSpecs) {
                FRendererPassRegistration registration{};
                registration.mPassId.Assign(spec.Id);
                registration.mPassSet     = spec.PassSet;
                registration.mAnchorOrder = spec.AnchorMode;
                if (spec.AnchorId != nullptr) {
                    registration.mAnchorPassId.Assign(spec.AnchorId);
                }
                registration.mExecute = [](AltinaEngine::RenderCore::FFrameGraph& graph) {
                    (void)graph;
                };
                RegisterPassToSet(registration);
            }
        }

    private:
        TVector<FPassSpec> mSpecs{};
    };

    struct FPluginProvider final : public IRendererPassProvider {
        explicit FPluginProvider(const TVector<FTestRenderer::FPassSpec>& inSpecs)
            : mSpecs(inSpecs) {}

        void RegisterPasses(FRendererBuilder& builder) override {
            for (const auto& spec : mSpecs) {
                FRendererPassRegistration registration{};
                registration.mPassId.Assign(spec.Id);
                registration.mPassSet     = spec.PassSet;
                registration.mAnchorOrder = spec.AnchorMode;
                if (spec.AnchorId != nullptr) {
                    registration.mAnchorPassId.Assign(spec.AnchorId);
                }
                registration.mExecute = [](AltinaEngine::RenderCore::FFrameGraph& graph) {
                    (void)graph;
                };
                builder.RegisterPluginPass(registration);
            }
        }

    private:
        TVector<FTestRenderer::FPassSpec> mSpecs{};
    };
} // namespace

TEST_CASE("Rendering.BaseRenderer.PassSetOrderStable") {
    TVector<FTestRenderer::FPassSpec> builtinSpecs{};
    builtinSpecs.PushBack({ TEXT("Builtin.Prepass"), ERendererPassSet::Prepass,
        ERendererPassAnchorOrder::None, nullptr });
    builtinSpecs.PushBack({ TEXT("Builtin.Shadow"), ERendererPassSet::Shadow,
        ERendererPassAnchorOrder::None, nullptr });
    builtinSpecs.PushBack({ TEXT("Builtin.Deferred"), ERendererPassSet::DeferredBase,
        ERendererPassAnchorOrder::None, nullptr });
    builtinSpecs.PushBack({ TEXT("Builtin.Post"), ERendererPassSet::PostProcess,
        ERendererPassAnchorOrder::None, nullptr });

    FTestRenderer    renderer(builtinSpecs);
    FRendererBuilder builder{};
    builder.ApplyToRenderer(renderer);

    const auto resolvedPasses = renderer.GetResolvedPassIds();
    REQUIRE(resolvedPasses.Size() == 4U);
    REQUIRE(resolvedPasses[0] == FString(TEXT("Builtin.Prepass")));
    REQUIRE(resolvedPasses[1] == FString(TEXT("Builtin.Shadow")));
    REQUIRE(resolvedPasses[2] == FString(TEXT("Builtin.Deferred")));
    REQUIRE(resolvedPasses[3] == FString(TEXT("Builtin.Post")));
}

TEST_CASE("Rendering.BaseRenderer.AnchorInsertStable") {
    TVector<FTestRenderer::FPassSpec> builtinSpecs{};
    builtinSpecs.PushBack({ TEXT("Builtin.Base"), ERendererPassSet::DeferredBase,
        ERendererPassAnchorOrder::None, nullptr });
    FTestRenderer                     renderer(builtinSpecs);

    TVector<FTestRenderer::FPassSpec> pluginSpecs{};
    pluginSpecs.PushBack({ TEXT("Plugin.Before"), ERendererPassSet::DeferredBase,
        ERendererPassAnchorOrder::Before, TEXT("Builtin.Base") });
    pluginSpecs.PushBack({ TEXT("Plugin.AfterA"), ERendererPassSet::DeferredBase,
        ERendererPassAnchorOrder::After, TEXT("Builtin.Base") });
    pluginSpecs.PushBack({ TEXT("Plugin.AfterB"), ERendererPassSet::DeferredBase,
        ERendererPassAnchorOrder::After, TEXT("Builtin.Base") });
    FPluginProvider  pluginProvider(pluginSpecs);

    FRendererBuilder builder{};
    builder.AddPassProvider(&pluginProvider);
    builder.ApplyToRenderer(renderer);

    const auto resolvedPasses = renderer.GetResolvedPassIds();
    REQUIRE(resolvedPasses.Size() == 4U);
    REQUIRE(resolvedPasses[0] == FString(TEXT("Plugin.Before")));
    REQUIRE(resolvedPasses[1] == FString(TEXT("Builtin.Base")));
    REQUIRE(resolvedPasses[2] == FString(TEXT("Plugin.AfterA")));
    REQUIRE(resolvedPasses[3] == FString(TEXT("Plugin.AfterB")));
}

TEST_CASE("Rendering.BaseRenderer.InvalidPluginPassIgnored") {
    TVector<FTestRenderer::FPassSpec> builtinSpecs{};
    builtinSpecs.PushBack({ TEXT("Builtin.AnchorPre"), ERendererPassSet::Prepass,
        ERendererPassAnchorOrder::None, nullptr });
    builtinSpecs.PushBack({ TEXT("Builtin.AnchorDeferred"), ERendererPassSet::DeferredBase,
        ERendererPassAnchorOrder::None, nullptr });
    FTestRenderer                     renderer(builtinSpecs);

    TVector<FTestRenderer::FPassSpec> pluginSpecs{};
    pluginSpecs.PushBack({ TEXT("Builtin.AnchorPre"), ERendererPassSet::Prepass,
        ERendererPassAnchorOrder::None, nullptr });
    pluginSpecs.PushBack({ TEXT("Plugin.MissingAnchor"), ERendererPassSet::DeferredBase,
        ERendererPassAnchorOrder::After, TEXT("Plugin.NoSuchAnchor") });
    pluginSpecs.PushBack({ TEXT("Plugin.CrossSet"), ERendererPassSet::PostProcess,
        ERendererPassAnchorOrder::After, TEXT("Builtin.AnchorPre") });
    pluginSpecs.PushBack({ TEXT("Plugin.Valid"), ERendererPassSet::Prepass,
        ERendererPassAnchorOrder::After, TEXT("Builtin.AnchorPre") });
    FPluginProvider  pluginProvider(pluginSpecs);

    FRendererBuilder builder{};
    builder.AddPassProvider(&pluginProvider);
    builder.ApplyToRenderer(renderer);

    const auto resolvedPasses = renderer.GetResolvedPassIds();
    REQUIRE(ContainsId(resolvedPasses, TEXT("Builtin.AnchorPre")));
    REQUIRE(ContainsId(resolvedPasses, TEXT("Builtin.AnchorDeferred")));
    REQUIRE(ContainsId(resolvedPasses, TEXT("Plugin.Valid")));
    REQUIRE(!ContainsId(resolvedPasses, TEXT("Plugin.MissingAnchor")));
    REQUIRE(!ContainsId(resolvedPasses, TEXT("Plugin.CrossSet")));
}

TEST_CASE("Rendering.BasicDeferredRenderer.PassRegistrationSnapshot") {
    FBasicDeferredRenderer deferredRenderer{};
    FRendererBuilder       builder{};
    builder.ApplyToRenderer(deferredRenderer);

    const auto resolvedPasses = deferredRenderer.GetResolvedPassIds();
    REQUIRE(resolvedPasses.Size() == 6U);
    REQUIRE(resolvedPasses[0] == FString(TEXT("Deferred.CsmShadow")));
    REQUIRE(resolvedPasses[1] == FString(TEXT("Deferred.GBufferBase")));
    REQUIRE(resolvedPasses[2] == FString(TEXT("Deferred.Ssao")));
    REQUIRE(resolvedPasses[3] == FString(TEXT("Deferred.Lighting")));
    REQUIRE(resolvedPasses[4] == FString(TEXT("Deferred.Sky")));
    REQUIRE(resolvedPasses[5] == FString(TEXT("Deferred.PostProcess")));
}

TEST_CASE("Rendering.GraphicsPipelinePreset.FullscreenDefaults") {
    FRendererGraphicsPipelineBuildInputs inputs{};
    inputs.mPreset    = ERendererGraphicsPipelinePreset::Fullscreen;
    inputs.mDebugName = TEXT("Tests.FullscreenPreset");

    AltinaEngine::Rhi::FRhiGraphicsPipelineDesc desc{};
    REQUIRE(BuildGraphicsPipelineDesc(inputs, desc));
    REQUIRE(desc.mRasterState.mCullMode == AltinaEngine::Rhi::ERhiRasterCullMode::None);
    REQUIRE(desc.mDepthState.mDepthEnable == false);
    REQUIRE(desc.mDepthState.mDepthWrite == false);
    REQUIRE(desc.mBlendState.mBlendEnable == false);
}

TEST_CASE("Rendering.GraphicsPipelinePreset.ShadowDepthUsesMaterialState") {
    FMaterialPassState materialState{};
    materialState.mRaster.mCullMode    = AltinaEngine::Rhi::ERhiRasterCullMode::Front;
    materialState.mDepth.mDepthEnable  = true;
    materialState.mDepth.mDepthWrite   = true;
    materialState.mDepth.mDepthCompare = AltinaEngine::Rhi::ERhiCompareOp::LessEqual;
    materialState.mBlend.mBlendEnable  = true;
    materialState.mBlend.mSrcColor     = AltinaEngine::Rhi::ERhiBlendFactor::One;
    materialState.mBlend.mDstColor     = AltinaEngine::Rhi::ERhiBlendFactor::One;
    materialState.mBlend.mColorOp      = AltinaEngine::Rhi::ERhiBlendOp::Add;
    materialState.mBlend.mSrcAlpha     = AltinaEngine::Rhi::ERhiBlendFactor::One;
    materialState.mBlend.mDstAlpha     = AltinaEngine::Rhi::ERhiBlendFactor::One;
    materialState.mBlend.mAlphaOp      = AltinaEngine::Rhi::ERhiBlendOp::Add;

    FRendererGraphicsPipelineBuildInputs inputs{};
    inputs.mPreset            = ERendererGraphicsPipelinePreset::ShadowDepth;
    inputs.mDebugName         = TEXT("Tests.ShadowDepthPreset");
    inputs.mMaterialPassState = &materialState;

    AltinaEngine::Rhi::FRhiGraphicsPipelineDesc desc{};
    REQUIRE(BuildGraphicsPipelineDesc(inputs, desc));
    REQUIRE(desc.mRasterState.mCullMode == AltinaEngine::Rhi::ERhiRasterCullMode::Front);
    REQUIRE(desc.mDepthState.mDepthEnable == true);
    REQUIRE(desc.mDepthState.mDepthWrite == true);
    REQUIRE(desc.mDepthState.mDepthCompare == AltinaEngine::Rhi::ERhiCompareOp::LessEqual);
    REQUIRE(desc.mBlendState.mBlendEnable == true);
}
