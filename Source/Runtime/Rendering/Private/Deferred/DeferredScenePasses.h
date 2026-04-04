#pragma once

#include "Deferred/DeferredTypes.h"

#include "FrameGraph/FrameGraph.h"
#include "Shader/ShaderBindingUtility.h"
#include "View/ViewData.h"

#include "Rhi/RhiBindGroupLayout.h"
#include "Rhi/RhiBuffer.h"
#include "Rhi/RhiPipeline.h"
#include "Rhi/RhiRefs.h"
#include "Rhi/RhiSampler.h"
#include "Rhi/RhiTexture.h"

namespace AltinaEngine::Rendering::Deferred {
    struct FLightingPassRuntimeSettings {
        bool bEnableIbl           = false;
        f32  IblDiffuseIntensity  = 0.0f;
        f32  IblSpecularIntensity = 0.0f;
        f32  IblSaturation        = 1.0f;
        f32  SpecularMaxLod       = 0.0f;
    };

    struct FDeferredLightingPassInputs {
        RenderCore::FFrameGraph*                              Graph             = nullptr;
        const RenderCore::View::FViewRect*                    ViewRect          = nullptr;
        const FPerFrameConstants*                             PerFrameConstants = nullptr;

        Rhi::FRhiPipeline*                                    Pipeline = nullptr;
        Rhi::FRhiBindGroupLayout*                             Layout   = nullptr;
        Rhi::FRhiSampler*                                     Sampler  = nullptr;
        const RenderCore::ShaderBinding::FBindingLookupTable* Bindings = nullptr;

        Rhi::FRhiBuffer*                                      PerFrameBuffer     = nullptr;
        Rhi::FRhiBuffer*                                      IblConstantsBuffer = nullptr;

        RenderCore::FFrameGraphTextureRef                     GBufferA;
        RenderCore::FFrameGraphTextureRef                     GBufferB;
        RenderCore::FFrameGraphTextureRef                     GBufferC;
        RenderCore::FFrameGraphTextureRef                     SceneDepth;
        RenderCore::FFrameGraphTextureRef                     SsaoTexture;
        RenderCore::FFrameGraphTextureRef                     ShadowMap;

        u32                                                   Width  = 0U;
        u32                                                   Height = 0U;

        Rhi::FRhiTextureRef                                   SkyIrradiance;
        Rhi::FRhiTextureRef                                   SkySpecular;
        Rhi::FRhiTextureRef                                   BrdfLut;
        Rhi::FRhiTextureRef                                   IblBlackCube;
        Rhi::FRhiTextureRef                                   IblBlack2D;
        FLightingPassRuntimeSettings                          RuntimeSettings{};
    };

    struct FDeferredSkyBoxPassInputs {
        RenderCore::FFrameGraph*                              Graph          = nullptr;
        const RenderCore::View::FViewRect*                    ViewRect       = nullptr;
        Rhi::FRhiPipeline*                                    Pipeline       = nullptr;
        Rhi::FRhiBindGroupLayout*                             Layout         = nullptr;
        Rhi::FRhiSampler*                                     Sampler        = nullptr;
        const RenderCore::ShaderBinding::FBindingLookupTable* Bindings       = nullptr;
        Rhi::FRhiBuffer*                                      PerFrameBuffer = nullptr;
        Rhi::FRhiTextureRef                                   SkyCube;
        RenderCore::FFrameGraphTextureRef                     SceneDepth;
        RenderCore::FFrameGraphTextureRef                     SceneColorHDR;
    };

    struct FDeferredAtmosphereSkyPassInputs {
        RenderCore::FFrameGraph*                              Graph                  = nullptr;
        const RenderCore::View::FViewRect*                    ViewRect               = nullptr;
        Rhi::FRhiPipeline*                                    Pipeline               = nullptr;
        Rhi::FRhiBindGroupLayout*                             Layout                 = nullptr;
        Rhi::FRhiSampler*                                     Sampler                = nullptr;
        const RenderCore::ShaderBinding::FBindingLookupTable* Bindings               = nullptr;
        Rhi::FRhiBuffer*                                      PerFrameBuffer         = nullptr;
        Rhi::FRhiBuffer*                                      AtmosphereParamsBuffer = nullptr;
        Rhi::FRhiTextureRef                                   TransmittanceLut;
        Rhi::FRhiTextureRef                                   ScatteringLut;
        Rhi::FRhiTextureRef                                   SingleMieScatteringLut;
        RenderCore::FFrameGraphTextureRef                     SceneDepth;
        RenderCore::FFrameGraphTextureRef                     SceneColorHDR;
    };

    void AddDeferredLightingPass(
        FDeferredLightingPassInputs& inputs, RenderCore::FFrameGraphTextureRef& outSceneColorHDR);
    void AddDeferredSkyBoxPass(FDeferredSkyBoxPassInputs& inputs);
    void AddDeferredAtmosphereSkyPass(FDeferredAtmosphereSkyPassInputs& inputs);
} // namespace AltinaEngine::Rendering::Deferred
