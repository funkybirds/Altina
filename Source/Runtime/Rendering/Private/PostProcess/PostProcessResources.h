#pragma once

#include "Container/SmartPtr.h"
#include "Container/StringView.h"
#include "Rhi/RhiRefs.h"
#include "Types/Aliases.h"

namespace AltinaEngine::Rhi {
    class FRhiDevice;
}

namespace AltinaEngine::Rendering::PostProcess::Detail {
    // Must match Source/Shader/PostProcess/PostProcess.hlsl cbuffer layout.
    struct alignas(16) FPostProcessConstants {
        f32 Exposure             = 1.0f;
        f32 Gamma                = 2.2f;
        f32 FxaaEdgeThreshold    = 0.125f;
        f32 FxaaEdgeThresholdMin = 0.0312f;
        f32 FxaaSubpix           = 0.75f;
        f32 _pad0                = 0.0f;
        f32 _pad1                = 0.0f;
        f32 _pad2                = 0.0f;
    };

    struct FPostProcessSharedResources {
        Rhi::FRhiShaderRef          FullscreenVS;
        Rhi::FRhiShaderRef          BlitPS;
        Rhi::FRhiShaderRef          TonemapPS;
        Rhi::FRhiShaderRef          FxaaPS;

        Rhi::FRhiBindGroupLayoutRef Layout;
        Rhi::FRhiPipelineLayoutRef  PipelineLayout;
        Rhi::FRhiSamplerRef         LinearSampler;

        Rhi::FRhiPipelineRef        BlitPipeline;
        Rhi::FRhiPipelineRef        TonemapPipeline;
        Rhi::FRhiPipelineRef        FxaaPipeline;

        Rhi::FRhiBufferRef          ConstantsBuffer;
    };

    [[nodiscard]] auto GetPostProcessSharedResources() -> FPostProcessSharedResources&;

    // Compiles shaders + creates pipelines/layouts/buffers lazily.
    [[nodiscard]] auto EnsurePostProcessSharedResources() -> bool;
} // namespace AltinaEngine::Rendering::PostProcess::Detail
