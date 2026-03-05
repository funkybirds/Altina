#pragma once

#include "Container/SmartPtr.h"
#include "Container/StringView.h"
#include "Math/Matrix.h"
#include "Rhi/RhiRefs.h"
#include "Rhi/RhiInit.h"
#include "Shader/ShaderBindingUtility.h"
#include "Types/Aliases.h"

namespace AltinaEngine::Rhi {
    class FRhiDevice;
}

namespace AltinaEngine::Rendering::PostProcess::Detail {
    inline constexpr auto kNameSceneColor       = TEXT("SceneColor");
    inline constexpr auto kNameLinearSampler    = TEXT("LinearSampler");
    inline constexpr auto kNameBlitConstants    = TEXT("BlitConstants");
    inline constexpr auto kNameTonemapConstants = TEXT("TonemapConstants");
    inline constexpr auto kNameFxaaConstants    = TEXT("FxaaConstants");
    inline constexpr auto kNameBloomConstants   = TEXT("BloomConstants");
    inline constexpr auto kNameTaaConstants     = TEXT("TaaConstants");
    inline constexpr auto kNameCurrentColor     = TEXT("CurrentColor");
    inline constexpr auto kNameHistoryColor     = TEXT("HistoryColor");
    inline constexpr auto kNameSceneDepth       = TEXT("SceneDepth");

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

    // Must match Source/Shader/PostProcess/Taa.hlsl cbuffer layout.
    struct alignas(16) FTaaConstants {
        f32                     RenderTargetSize[2]    = { 0.0f, 0.0f };
        f32                     InvRenderTargetSize[2] = { 0.0f, 0.0f };

        f32                     JitterNdc[2]     = { 0.0f, 0.0f };
        f32                     PrevJitterNdc[2] = { 0.0f, 0.0f };

        f32                     Alpha       = 0.9f;
        f32                     ClampK      = 1.0f;
        u32                     bHasHistory = 0U;
        u32                     _pad0       = 0U;

        Core::Math::FMatrix4x4f InvViewProjJittered{};
        Core::Math::FMatrix4x4f PrevViewProjJittered{};
    };

    struct FPostProcessSharedResources {
        Rhi::FRhiShaderRef                             FullscreenVS;
        Rhi::FRhiShaderRef                             BlitPS;
        Rhi::FRhiShaderRef                             TonemapPS;
        Rhi::FRhiShaderRef                             FxaaPS;
        Rhi::FRhiShaderRef                             BloomPrefilterPS;
        Rhi::FRhiShaderRef                             BloomDownsamplePS;
        Rhi::FRhiShaderRef                             BloomDownsampleWeightedPS;
        Rhi::FRhiShaderRef                             BloomBlurHPS;
        Rhi::FRhiShaderRef                             BloomBlurVPS;
        Rhi::FRhiShaderRef                             BloomUpsamplePS;
        Rhi::FRhiShaderRef                             BloomApplyPS;
        Rhi::FRhiShaderRef                             TaaPS;

        Rhi::FRhiBindGroupLayoutRef                    Layout;
        Rhi::FRhiBindGroupLayoutRef                    TaaLayout;
        RenderCore::ShaderBinding::FBindingLookupTable LayoutBindings;
        RenderCore::ShaderBinding::FBindingLookupTable TaaLayoutBindings;
        Rhi::FRhiPipelineLayoutRef                     PipelineLayout;
        Rhi::FRhiPipelineLayoutRef                     TaaPipelineLayout;
        Rhi::FRhiSamplerRef                            LinearSampler;

        Rhi::FRhiPipelineRef                           BlitPipeline;
        Rhi::FRhiPipelineRef                           TonemapPipeline;
        Rhi::FRhiPipelineRef                           FxaaPipeline;
        Rhi::FRhiPipelineRef                           BloomPrefilterPipeline;
        Rhi::FRhiPipelineRef                           BloomDownsamplePipeline;
        Rhi::FRhiPipelineRef                           BloomDownsampleWeightedPipeline;
        Rhi::FRhiPipelineRef                           BloomBlurHPipeline;
        Rhi::FRhiPipelineRef                           BloomBlurVPipeline;
        Rhi::FRhiPipelineRef                           BloomUpsampleAddPipeline;
        Rhi::FRhiPipelineRef                           BloomApplyAddPipeline;
        Rhi::FRhiPipelineRef                           TaaPipeline;

        Rhi::FRhiBufferRef                             BlitConstantsBuffer;
        Rhi::FRhiBufferRef                             TonemapConstantsBuffer;
        Rhi::FRhiBufferRef                             FxaaConstantsBuffer;
        Rhi::FRhiBufferRef                             BloomConstantsBuffer;
        Rhi::FRhiBufferRef                             TaaConstantsBuffer;
    };

    [[nodiscard]] auto GetPostProcessSharedResources() -> FPostProcessSharedResources&;

    // Compiles shaders + creates pipelines/layouts/buffers lazily.
    [[nodiscard]] auto EnsurePostProcessSharedResources() -> bool;
    [[nodiscard]] auto BuildCommonBindGroupDesc(const FPostProcessSharedResources& resources,
        const TChar* cbufferName, Rhi::FRhiBuffer* cbuffer, u64 cbufferSize,
        Rhi::FRhiTexture* texture, Rhi::FRhiBindGroupDesc& outDesc) -> bool;
    [[nodiscard]] auto BuildTaaBindGroupDesc(const FPostProcessSharedResources& resources,
        Rhi::FRhiTexture* currentColor, Rhi::FRhiTexture* historyColor,
        Rhi::FRhiTexture* sceneDepth, Rhi::FRhiBindGroupDesc& outDesc) -> bool;
    void               ShutdownPostProcessSharedResources() noexcept;
} // namespace AltinaEngine::Rendering::PostProcess::Detail
