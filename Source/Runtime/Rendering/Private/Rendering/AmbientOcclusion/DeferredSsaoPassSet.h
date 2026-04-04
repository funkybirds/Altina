#pragma once

#include "Deferred/DeferredCsm.h"
#include "Deferred/DeferredSsaoPass.h"

#include "Lighting/LightTypes.h"
#include "Rendering/RenderingSettings.h"
#include "View/ViewData.h"

#include "Math/Common.h"
#include "Math/Vector.h"

namespace AltinaEngine::Rendering::AmbientOcclusion {
    using Core::Math::FVector3f;

    struct FDeferredSsaoPassSetInputs {
        RenderCore::FFrameGraph*                              mGraph  = nullptr;
        const RenderCore::View::FViewData*                    mView   = nullptr;
        const RenderCore::Lighting::FLightSceneData*          mLights = nullptr;

        Rhi::FRhiPipeline*                                    mPipeline            = nullptr;
        Rhi::FRhiBindGroupLayout*                             mLayout              = nullptr;
        Rhi::FRhiSampler*                                     mSampler             = nullptr;
        const RenderCore::ShaderBinding::FBindingLookupTable* mBindings            = nullptr;
        Rhi::FRhiBuffer*                                      mPerFrameBuffer      = nullptr;
        Rhi::FRhiBuffer*                                      mSsaoConstantsBuffer = nullptr;

        RenderCore::FFrameGraphTextureRef                     mGBufferB{};
        RenderCore::FFrameGraphTextureRef                     mSceneDepth{};
        u32                                                   mDebugShadingMode = 0U;
    };

    inline void BuildDeferredPerFrameConstantsForSsao(const RenderCore::View::FViewData& view,
        const RenderCore::Lighting::FLightSceneData* lights, const Deferred::FCsmBuildResult& csm,
        u32 debugShadingMode, Deferred::FPerFrameConstants& outPerFrameConstants) {
        outPerFrameConstants.ViewProjection = view.Matrices.ViewProjJittered;
        outPerFrameConstants.View           = view.Matrices.View;
        outPerFrameConstants.Proj           = view.Matrices.ProjJittered;
        outPerFrameConstants.ViewProj       = view.Matrices.ViewProjJittered;
        outPerFrameConstants.InvViewProj    = view.Matrices.InvViewProjJittered;

        outPerFrameConstants.ViewOriginWS[0]  = view.ViewOrigin[0];
        outPerFrameConstants.ViewOriginWS[1]  = view.ViewOrigin[1];
        outPerFrameConstants.ViewOriginWS[2]  = view.ViewOrigin[2];
        outPerFrameConstants.bReverseZ        = view.bReverseZ ? 1U : 0U;
        outPerFrameConstants.DebugShadingMode = debugShadingMode;

        const f32 w                              = static_cast<f32>(view.RenderTargetExtent.Width);
        const f32 h                              = static_cast<f32>(view.RenderTargetExtent.Height);
        outPerFrameConstants.RenderTargetSize[0] = w;
        outPerFrameConstants.RenderTargetSize[1] = h;
        outPerFrameConstants.InvRenderTargetSize[0] = (w > 0.0f) ? (1.0f / w) : 0.0f;
        outPerFrameConstants.InvRenderTargetSize[1] = (h > 0.0f) ? (1.0f / h) : 0.0f;

        RenderCore::Lighting::FDirectionalLight dir{};
        if (lights != nullptr && lights->mHasMainDirectionalLight) {
            dir = lights->mMainDirectionalLight;
        } else {
            dir.mDirectionWS = FVector3f(0.4f, 0.6f, 0.7f);
            dir.mColor       = FVector3f(1.0f, 1.0f, 1.0f);
            dir.mIntensity   = 2.0f;
            dir.mCastShadows = false;
        }

        outPerFrameConstants.DirLightDirectionWS[0] = dir.mDirectionWS[0];
        outPerFrameConstants.DirLightDirectionWS[1] = dir.mDirectionWS[1];
        outPerFrameConstants.DirLightDirectionWS[2] = dir.mDirectionWS[2];
        outPerFrameConstants.DirLightColor[0]       = dir.mColor[0];
        outPerFrameConstants.DirLightColor[1]       = dir.mColor[1];
        outPerFrameConstants.DirLightColor[2]       = dir.mColor[2];
        outPerFrameConstants.DirLightIntensity      = dir.mIntensity;

        outPerFrameConstants.PointLightCount = 0U;
        if (lights != nullptr && !lights->mPointLights.IsEmpty()) {
            const u32 count = static_cast<u32>(lights->mPointLights.Size());
            const u32 clamped =
                (count > Deferred::kMaxPointLights) ? Deferred::kMaxPointLights : count;
            outPerFrameConstants.PointLightCount = clamped;
            for (u32 i = 0U; i < clamped; ++i) {
                const auto& src                                   = lights->mPointLights[i];
                outPerFrameConstants.PointLights[i].PositionWS[0] = src.mPositionWS[0];
                outPerFrameConstants.PointLights[i].PositionWS[1] = src.mPositionWS[1];
                outPerFrameConstants.PointLights[i].PositionWS[2] = src.mPositionWS[2];
                outPerFrameConstants.PointLights[i].Range         = src.mRange;
                outPerFrameConstants.PointLights[i].Color[0]      = src.mColor[0];
                outPerFrameConstants.PointLights[i].Color[1]      = src.mColor[1];
                outPerFrameConstants.PointLights[i].Color[2]      = src.mColor[2];
                outPerFrameConstants.PointLights[i].Intensity     = src.mIntensity;
            }
        }

        Deferred::FillPerFrameCsmConstants(csm, outPerFrameConstants);
    }

    inline auto AddDeferredSsaoPassSet(const FDeferredSsaoPassSetInputs& inputs,
        RenderCore::FFrameGraphTextureRef&                               outSsaoTexture) -> bool {
        if (inputs.mGraph == nullptr || inputs.mView == nullptr || inputs.mPipeline == nullptr
            || inputs.mLayout == nullptr || inputs.mSampler == nullptr
            || inputs.mBindings == nullptr || !inputs.mGBufferB.IsValid()
            || !inputs.mSceneDepth.IsValid()) {
            return false;
        }

        Deferred::FCsmBuildInputs csmInputs{};
        csmInputs.View                      = inputs.mView;
        csmInputs.Lights                    = inputs.mLights;
        const Deferred::FCsmBuildResult csm = Deferred::BuildCsm(csmInputs);

        Deferred::FPerFrameConstants    perFrameConstants{};
        BuildDeferredPerFrameConstantsForSsao(
            *inputs.mView, inputs.mLights, csm, inputs.mDebugShadingMode, perFrameConstants);

        Deferred::FDeferredSsaoPassInputs ssaoInputs{};
        ssaoInputs.Graph                  = inputs.mGraph;
        ssaoInputs.ViewRect               = &inputs.mView->ViewRect;
        ssaoInputs.PerFrameConstants      = &perFrameConstants;
        ssaoInputs.Pipeline               = inputs.mPipeline;
        ssaoInputs.Layout                 = inputs.mLayout;
        ssaoInputs.Sampler                = inputs.mSampler;
        ssaoInputs.Bindings               = inputs.mBindings;
        ssaoInputs.PerFrameBuffer         = inputs.mPerFrameBuffer;
        ssaoInputs.SsaoConstantsBuffer    = inputs.mSsaoConstantsBuffer;
        ssaoInputs.GBufferB               = inputs.mGBufferB;
        ssaoInputs.SceneDepth             = inputs.mSceneDepth;
        ssaoInputs.Width                  = inputs.mView->RenderTargetExtent.Width;
        ssaoInputs.Height                 = inputs.mView->RenderTargetExtent.Height;
        ssaoInputs.RuntimeSettings.Enable = (rSsaoEnable.GetRenderValue() != 0) ? 1U : 0U;
        ssaoInputs.RuntimeSettings.SampleCount =
            static_cast<u32>(Core::Math::Max(0, rSsaoSampleCount.GetRenderValue()));
        ssaoInputs.RuntimeSettings.RadiusVS  = rSsaoRadiusVS.GetRenderValue();
        ssaoInputs.RuntimeSettings.BiasNdc   = rSsaoBiasNdc.GetRenderValue();
        ssaoInputs.RuntimeSettings.Power     = rSsaoPower.GetRenderValue();
        ssaoInputs.RuntimeSettings.Intensity = rSsaoIntensity.GetRenderValue();

        Deferred::AddDeferredSsaoPass(ssaoInputs, outSsaoTexture);
        return true;
    }
} // namespace AltinaEngine::Rendering::AmbientOcclusion
