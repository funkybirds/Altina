#pragma once

#include "Container/SmartPtr.h"
#include "Container/StringView.h"
#include "Rhi/RhiRefs.h"
#include "Types/Aliases.h"

namespace AltinaEngine::Rhi {
    class FRhiDevice;
}

namespace AltinaEngine::Rendering::PostProcess::Detail {
    // Constant buffer layouts (b0) for each post-process pass. Keep each struct size a multiple of
    // 16 bytes to match HLSL packing and avoid /WX padding warnings.

    // Must match Source/Shader/PostProcess/Blit.hlsl cbuffer layout.
    struct alignas(16) FBlitConstants {
        f32 Dummy0 = 0.0f;
        f32 Dummy1 = 0.0f;
        f32 Dummy2 = 0.0f;
        f32 Dummy3 = 0.0f;
    };

    // Must match Source/Shader/PostProcess/Tonemap.hlsl cbuffer layout.
    struct alignas(16) FTonemapConstants {
        f32 Exposure = 1.0f;
        f32 Gamma    = 2.2f;
        f32 _pad0    = 0.0f;
        f32 _pad1    = 0.0f;
    };

    // Must match Source/Shader/PostProcess/Fxaa.hlsl cbuffer layout.
    struct alignas(16) FFxaaConstants {
        f32 EdgeThreshold    = 0.125f;
        f32 EdgeThresholdMin = 0.0312f;
        f32 Subpix           = 0.75f;
        f32 _pad0            = 0.0f;
    };

    // Must match Source/Shader/PostProcess/Bloom.hlsl cbuffer layout.
    struct alignas(16) FBloomConstants {
        f32 Threshold    = 1.0f;
        f32 Knee         = 0.5f;
        f32 Intensity    = 0.05f;
        f32 KawaseOffset = 1.0f;
    };

    struct FPostProcessSharedResources {
        Rhi::FRhiShaderRef          FullscreenVS;
        Rhi::FRhiShaderRef          BlitPS;
        Rhi::FRhiShaderRef          TonemapPS;
        Rhi::FRhiShaderRef          FxaaPS;
        Rhi::FRhiShaderRef          BloomPrefilterPS;
        Rhi::FRhiShaderRef          BloomDownsamplePS;
        Rhi::FRhiShaderRef          BloomUpsamplePS;
        Rhi::FRhiShaderRef          BloomApplyPS;

        Rhi::FRhiBindGroupLayoutRef Layout;
        Rhi::FRhiPipelineLayoutRef  PipelineLayout;
        Rhi::FRhiSamplerRef         LinearSampler;

        Rhi::FRhiPipelineRef        BlitPipeline;
        Rhi::FRhiPipelineRef        TonemapPipeline;
        Rhi::FRhiPipelineRef        FxaaPipeline;
        Rhi::FRhiPipelineRef        BloomPrefilterPipeline;
        Rhi::FRhiPipelineRef        BloomDownsamplePipeline;
        Rhi::FRhiPipelineRef        BloomUpsampleAddPipeline;
        Rhi::FRhiPipelineRef        BloomApplyAddPipeline;

        Rhi::FRhiBufferRef          BlitConstantsBuffer;
        Rhi::FRhiBufferRef          TonemapConstantsBuffer;
        Rhi::FRhiBufferRef          FxaaConstantsBuffer;
        Rhi::FRhiBufferRef          BloomConstantsBuffer;
    };

    [[nodiscard]] auto GetPostProcessSharedResources() -> FPostProcessSharedResources&;

    // Compiles shaders + creates pipelines/layouts/buffers lazily.
    [[nodiscard]] auto EnsurePostProcessSharedResources() -> bool;
} // namespace AltinaEngine::Rendering::PostProcess::Detail
