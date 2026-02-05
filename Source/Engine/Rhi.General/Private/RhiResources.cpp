#include "Rhi/RhiBindGroup.h"
#include "Rhi/RhiBindGroupLayout.h"
#include "Rhi/RhiBuffer.h"
#include "Rhi/RhiCommandPool.h"
#include "Rhi/RhiFence.h"
#include "Rhi/RhiPipeline.h"
#include "Rhi/RhiPipelineLayout.h"
#include "Rhi/RhiSampler.h"
#include "Rhi/RhiSemaphore.h"
#include "Rhi/RhiShader.h"
#include "Rhi/RhiTexture.h"

namespace AltinaEngine::Rhi {

    FRhiBuffer::FRhiBuffer(const FRhiBufferDesc& desc, FRhiResourceDeleteQueue* deleteQueue) noexcept
        : FRhiResource(deleteQueue), mDesc(desc) {}

    FRhiBuffer::~FRhiBuffer() = default;

    FRhiTexture::FRhiTexture(const FRhiTextureDesc& desc, FRhiResourceDeleteQueue* deleteQueue) noexcept
        : FRhiResource(deleteQueue), mDesc(desc) {}

    FRhiTexture::~FRhiTexture() = default;

    FRhiSampler::FRhiSampler(const FRhiSamplerDesc& desc, FRhiResourceDeleteQueue* deleteQueue) noexcept
        : FRhiResource(deleteQueue), mDesc(desc) {}

    FRhiSampler::~FRhiSampler() = default;

    FRhiShader::FRhiShader(const FRhiShaderDesc& desc, FRhiResourceDeleteQueue* deleteQueue) noexcept
        : FRhiResource(deleteQueue), mDesc(desc) {}

    FRhiShader::~FRhiShader() = default;

    FRhiPipeline::FRhiPipeline(const FRhiGraphicsPipelineDesc& desc,
        FRhiResourceDeleteQueue* deleteQueue) noexcept
        : FRhiResource(deleteQueue), mGraphicsDesc(desc), mIsGraphics(true) {}

    FRhiPipeline::FRhiPipeline(const FRhiComputePipelineDesc& desc,
        FRhiResourceDeleteQueue* deleteQueue) noexcept
        : FRhiResource(deleteQueue), mComputeDesc(desc), mIsGraphics(false) {}

    FRhiPipeline::~FRhiPipeline() = default;

    FRhiPipelineLayout::FRhiPipelineLayout(const FRhiPipelineLayoutDesc& desc,
        FRhiResourceDeleteQueue* deleteQueue) noexcept
        : FRhiResource(deleteQueue), mDesc(desc) {}

    FRhiPipelineLayout::~FRhiPipelineLayout() = default;

    FRhiBindGroupLayout::FRhiBindGroupLayout(const FRhiBindGroupLayoutDesc& desc,
        FRhiResourceDeleteQueue* deleteQueue) noexcept
        : FRhiResource(deleteQueue), mDesc(desc) {}

    FRhiBindGroupLayout::~FRhiBindGroupLayout() = default;

    FRhiBindGroup::FRhiBindGroup(const FRhiBindGroupDesc& desc, FRhiResourceDeleteQueue* deleteQueue) noexcept
        : FRhiResource(deleteQueue), mDesc(desc) {}

    FRhiBindGroup::~FRhiBindGroup() = default;

    FRhiFence::FRhiFence(FRhiResourceDeleteQueue* deleteQueue) noexcept : FRhiResource(deleteQueue) {}

    FRhiFence::~FRhiFence() = default;

    FRhiSemaphore::FRhiSemaphore(FRhiResourceDeleteQueue* deleteQueue) noexcept
        : FRhiResource(deleteQueue) {}

    FRhiSemaphore::~FRhiSemaphore() = default;

    FRhiCommandPool::FRhiCommandPool(const FRhiCommandPoolDesc& desc,
        FRhiResourceDeleteQueue* deleteQueue) noexcept
        : FRhiResource(deleteQueue), mDesc(desc) {}

    FRhiCommandPool::~FRhiCommandPool() = default;

} // namespace AltinaEngine::Rhi
