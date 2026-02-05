#pragma once

#include "Rhi/RhiFwd.h"
#include "Container/CountRef.h"

namespace AltinaEngine::Rhi {
    using Core::Container::TCountRef;

    using FRhiResourceRef        = TCountRef<FRhiResource>;
    using FRhiQueueRef           = TCountRef<FRhiQueue>;
    using FRhiBufferRef          = TCountRef<FRhiBuffer>;
    using FRhiTextureRef         = TCountRef<FRhiTexture>;
    using FRhiSamplerRef         = TCountRef<FRhiSampler>;
    using FRhiShaderRef          = TCountRef<FRhiShader>;
    using FRhiPipelineRef        = TCountRef<FRhiPipeline>;
    using FRhiPipelineLayoutRef  = TCountRef<FRhiPipelineLayout>;
    using FRhiBindGroupLayoutRef = TCountRef<FRhiBindGroupLayout>;
    using FRhiBindGroupRef       = TCountRef<FRhiBindGroup>;
    using FRhiFenceRef           = TCountRef<FRhiFence>;
    using FRhiSemaphoreRef       = TCountRef<FRhiSemaphore>;
    using FRhiCommandPoolRef     = TCountRef<FRhiCommandPool>;
} // namespace AltinaEngine::Rhi
