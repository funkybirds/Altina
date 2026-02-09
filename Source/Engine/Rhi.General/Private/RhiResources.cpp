#include "Rhi/RhiBindGroup.h"
#include "Rhi/RhiBindGroupLayout.h"
#include "Rhi/RhiBuffer.h"
#include "Rhi/RhiCommandContext.h"
#include "Rhi/RhiCommandList.h"
#include "Rhi/RhiCommandPool.h"
#include "Rhi/RhiFence.h"
#include "Rhi/RhiPipeline.h"
#include "Rhi/RhiPipelineLayout.h"
#include "Rhi/RhiResourceView.h"
#include "Rhi/RhiSampler.h"
#include "Rhi/RhiSemaphore.h"
#include "Rhi/RhiShader.h"
#include "Rhi/RhiTexture.h"
#include "Rhi/RhiViewport.h"

namespace AltinaEngine::Rhi {

    FRhiBuffer::FRhiBuffer(const FRhiBufferDesc& desc, FRhiResourceDeleteQueue* deleteQueue) noexcept
        : FRhiResource(deleteQueue), mDesc(desc) {}

    FRhiBuffer::~FRhiBuffer() = default;

    auto FRhiBuffer::Lock(u64 /*offset*/, u64 /*size*/, ERhiBufferLockMode /*mode*/) -> FLockResult {
        return {};
    }

    void FRhiBuffer::Unlock(FLockResult& lock) {
        lock = {};
    }

    FRhiTexture::FRhiTexture(const FRhiTextureDesc& desc, FRhiResourceDeleteQueue* deleteQueue) noexcept
        : FRhiResource(deleteQueue), mDesc(desc) {}

    FRhiTexture::~FRhiTexture() = default;

    FRhiViewport::FRhiViewport(const FRhiViewportDesc& desc, FRhiResourceDeleteQueue* deleteQueue) noexcept
        : FRhiResource(deleteQueue), mDesc(desc) {}

    FRhiViewport::~FRhiViewport() = default;

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

    FRhiResourceView::FRhiResourceView(ERhiResourceViewType type,
        FRhiResourceDeleteQueue* deleteQueue) noexcept
        : FRhiResource(deleteQueue), mViewType(type) {}

    FRhiResourceView::~FRhiResourceView() = default;

    FRhiShaderResourceView::FRhiShaderResourceView(const FRhiShaderResourceViewDesc& desc,
        FRhiResourceDeleteQueue* deleteQueue) noexcept
        : FRhiResourceView(ERhiResourceViewType::ShaderResource, deleteQueue), mDesc(desc) {
        if (mDesc.mTexture) {
            mTexture.Reset(mDesc.mTexture);
        }
        if (mDesc.mBuffer) {
            mBuffer.Reset(mDesc.mBuffer);
        }
    }

    FRhiShaderResourceView::~FRhiShaderResourceView() = default;

    FRhiUnorderedAccessView::FRhiUnorderedAccessView(const FRhiUnorderedAccessViewDesc& desc,
        FRhiResourceDeleteQueue* deleteQueue) noexcept
        : FRhiResourceView(ERhiResourceViewType::UnorderedAccess, deleteQueue), mDesc(desc) {
        if (mDesc.mTexture) {
            mTexture.Reset(mDesc.mTexture);
        }
        if (mDesc.mBuffer) {
            mBuffer.Reset(mDesc.mBuffer);
        }
    }

    FRhiUnorderedAccessView::~FRhiUnorderedAccessView() = default;

    FRhiRenderTargetView::FRhiRenderTargetView(const FRhiRenderTargetViewDesc& desc,
        FRhiResourceDeleteQueue* deleteQueue) noexcept
        : FRhiResourceView(ERhiResourceViewType::RenderTarget, deleteQueue), mDesc(desc) {
        if (mDesc.mTexture) {
            mTexture.Reset(mDesc.mTexture);
        }
    }

    FRhiRenderTargetView::~FRhiRenderTargetView() = default;

    FRhiDepthStencilView::FRhiDepthStencilView(const FRhiDepthStencilViewDesc& desc,
        FRhiResourceDeleteQueue* deleteQueue) noexcept
        : FRhiResourceView(ERhiResourceViewType::DepthStencil, deleteQueue), mDesc(desc) {
        if (mDesc.mTexture) {
            mTexture.Reset(mDesc.mTexture);
        }
    }

    FRhiDepthStencilView::~FRhiDepthStencilView() = default;

    FRhiFence::FRhiFence(FRhiResourceDeleteQueue* deleteQueue) noexcept : FRhiResource(deleteQueue) {}

    FRhiFence::~FRhiFence() = default;

    FRhiSemaphore::FRhiSemaphore(FRhiResourceDeleteQueue* deleteQueue) noexcept
        : FRhiResource(deleteQueue) {}

    FRhiSemaphore::~FRhiSemaphore() = default;

    FRhiCommandPool::FRhiCommandPool(const FRhiCommandPoolDesc& desc,
        FRhiResourceDeleteQueue* deleteQueue) noexcept
        : FRhiResource(deleteQueue), mDesc(desc) {}

    FRhiCommandPool::~FRhiCommandPool() = default;

    FRhiCommandList::FRhiCommandList(const FRhiCommandListDesc& desc,
        FRhiResourceDeleteQueue* deleteQueue) noexcept
        : FRhiResource(deleteQueue), mDesc(desc) {}

    FRhiCommandList::~FRhiCommandList() = default;

    FRhiCommandContext::FRhiCommandContext(const FRhiCommandContextDesc& desc,
        FRhiResourceDeleteQueue* deleteQueue) noexcept
        : FRhiResource(deleteQueue), mDesc(desc) {}

    FRhiCommandContext::~FRhiCommandContext() = default;

} // namespace AltinaEngine::Rhi
