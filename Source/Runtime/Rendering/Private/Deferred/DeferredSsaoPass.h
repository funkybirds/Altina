#pragma once

#include "Deferred/DeferredTypes.h"

#include "FrameGraph/FrameGraph.h"
#include "Shader/ShaderBindingUtility.h"
#include "View/ViewData.h"

#include "Rhi/RhiBindGroupLayout.h"
#include "Rhi/RhiBuffer.h"
#include "Rhi/RhiPipeline.h"
#include "Rhi/RhiSampler.h"

namespace AltinaEngine::Rendering::Deferred {
    struct FSsaoRuntimeSettings {
        u32 Enable      = 1U;
        u32 SampleCount = 12U;
        f32 RadiusVS    = 0.55f;
        f32 BiasNdc     = 0.0005f;
        f32 Power       = 1.6f;
        f32 Intensity   = 1.0f;
    };

    struct FDeferredSsaoPassInputs {
        RenderCore::FFrameGraph*                              Graph             = nullptr;
        const RenderCore::View::FViewRect*                    ViewRect          = nullptr;
        const FPerFrameConstants*                             PerFrameConstants = nullptr;

        Rhi::FRhiPipeline*                                    Pipeline = nullptr;
        Rhi::FRhiBindGroupLayout*                             Layout   = nullptr;
        Rhi::FRhiSampler*                                     Sampler  = nullptr;
        const RenderCore::ShaderBinding::FBindingLookupTable* Bindings = nullptr;

        Rhi::FRhiBuffer*                                      PerFrameBuffer      = nullptr;
        Rhi::FRhiBuffer*                                      SsaoConstantsBuffer = nullptr;

        RenderCore::FFrameGraphTextureRef                     GBufferB;
        RenderCore::FFrameGraphTextureRef                     SceneDepth;
        u32                                                   Width  = 0U;
        u32                                                   Height = 0U;

        FSsaoRuntimeSettings                                  RuntimeSettings{};
    };

    void AddDeferredSsaoPass(
        FDeferredSsaoPassInputs& inputs, RenderCore::FFrameGraphTextureRef& outSsaoTexture);
} // namespace AltinaEngine::Rendering::Deferred
