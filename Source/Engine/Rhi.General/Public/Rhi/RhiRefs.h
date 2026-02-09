#pragma once

#include "Rhi/RhiFwd.h"
#include "Container/CountRef.h"

namespace AltinaEngine::Rhi {
    using Core::Container::TCountRef;

    // NOLINTBEGIN(*-identifier-naming)
    using FRhiResourceRef        = TCountRef<FRhiResource>;
    using FRhiQueueRef           = TCountRef<FRhiQueue>;
    using FRhiBufferRef          = TCountRef<FRhiBuffer>;
    using FRhiTextureRef         = TCountRef<FRhiTexture>;
    using FRhiViewportRef        = TCountRef<FRhiViewport>;
    using FRhiSamplerRef         = TCountRef<FRhiSampler>;
    using FRhiShaderRef          = TCountRef<FRhiShader>;
    using FRhiPipelineRef        = TCountRef<FRhiPipeline>;
    using FRhiPipelineLayoutRef  = TCountRef<FRhiPipelineLayout>;
    using FRhiBindGroupLayoutRef = TCountRef<FRhiBindGroupLayout>;
    using FRhiBindGroupRef       = TCountRef<FRhiBindGroup>;
    using FRhiResourceViewRef    = TCountRef<FRhiResourceView>;
    using FRhiShaderResourceViewRef = TCountRef<FRhiShaderResourceView>;
    using FRhiUnorderedAccessViewRef = TCountRef<FRhiUnorderedAccessView>;
    using FRhiRenderTargetViewRef = TCountRef<FRhiRenderTargetView>;
    using FRhiDepthStencilViewRef = TCountRef<FRhiDepthStencilView>;
    using FRhiFenceRef           = TCountRef<FRhiFence>;
    using FRhiSemaphoreRef       = TCountRef<FRhiSemaphore>;
    using FRhiCommandPoolRef     = TCountRef<FRhiCommandPool>;
    using FRhiCommandListRef     = TCountRef<FRhiCommandList>;
    using FRhiCommandContextRef  = TCountRef<FRhiCommandContext>;
    // NOLINTEND(*-identifier-naming)
} // namespace AltinaEngine::Rhi
