#pragma once

#include "RhiGeneralAPI.h"

namespace AltinaEngine::Rhi {
    class FRhiContext;
    class FRhiAdapter;
    class FRhiDevice;
    class FRhiResource;
    class FRhiResourceDeleteQueue;
    class FRhiQueue;
    class FRhiBuffer;
    class FRhiTexture;
    class FRhiSampler;
    class FRhiShader;
    class FRhiPipeline;
    class FRhiPipelineLayout;
    class FRhiBindGroupLayout;
    class FRhiBindGroup;
    class FRhiFence;
    class FRhiSemaphore;
    class FRhiCommandPool;
    class FRhiCommandList;
    class FRhiCommandContext;
    class FRhiSwapchain;

    struct FRhiInitDesc;
    struct FRhiAdapterDesc;
    struct FRhiDeviceDesc;
    struct FRhiSupportedFeatures;
    struct FRhiSupportedLimits;
    struct FRhiQueueCapabilities;
    struct FRhiBufferDesc;
    struct FRhiTextureDesc;
    struct FRhiSamplerDesc;
    struct FRhiShaderDesc;
    struct FRhiVertexAttributeDesc;
    struct FRhiVertexLayoutDesc;
    struct FRhiGraphicsPipelineDesc;
    struct FRhiComputePipelineDesc;
    struct FRhiPipelineLayoutDesc;
    struct FRhiBindGroupLayoutDesc;
    struct FRhiBindGroupDesc;
    struct FRhiCommandPoolDesc;
    struct FRhiCommandListDesc;
    struct FRhiCommandContextDesc;
    struct FRhiQueueWait;
    struct FRhiQueueSignal;
    struct FRhiSubmitInfo;
    struct FRhiPresentInfo;
} // namespace AltinaEngine::Rhi
