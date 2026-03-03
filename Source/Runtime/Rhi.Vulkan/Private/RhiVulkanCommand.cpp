
#include "RhiVulkan/RhiVulkanCommandContext.h"
#include "RhiVulkan/RhiVulkanCommandList.h"
#include "RhiVulkan/RhiVulkanCommandPool.h"
#include "RhiVulkan/RhiVulkanPipeline.h"
#include "RhiVulkan/RhiVulkanResources.h"
#include "RhiVulkan/RhiVulkanDevice.h"

#include "Rhi/RhiBindGroup.h"
#include "Rhi/RhiPipelineLayout.h"
#include "Rhi/RhiInit.h"
#include "Types/CheckedCast.h"
#include "Utility/Assert.h"

#include "RhiVulkanInternal.h"

using AltinaEngine::Move;
namespace AltinaEngine::Rhi {
    namespace Container = Core::Container;
    using Container::TVector;

    namespace {
        [[nodiscard]] auto ToVkIndexType(ERhiIndexType type) noexcept -> VkIndexType {
            return (type == ERhiIndexType::Uint16) ? VK_INDEX_TYPE_UINT16 : VK_INDEX_TYPE_UINT32;
        }

        [[nodiscard]] auto ToVkLoadOp(ERhiLoadOp op) noexcept -> VkAttachmentLoadOp {
            switch (op) {
                case ERhiLoadOp::Clear:
                    return VK_ATTACHMENT_LOAD_OP_CLEAR;
                case ERhiLoadOp::DontCare:
                    return VK_ATTACHMENT_LOAD_OP_DONT_CARE;
                case ERhiLoadOp::Load:
                default:
                    return VK_ATTACHMENT_LOAD_OP_LOAD;
            }
        }

        [[nodiscard]] auto ToVkStoreOp(ERhiStoreOp op) noexcept -> VkAttachmentStoreOp {
            switch (op) {
                case ERhiStoreOp::DontCare:
                    return VK_ATTACHMENT_STORE_OP_DONT_CARE;
                case ERhiStoreOp::Store:
                default:
                    return VK_ATTACHMENT_STORE_OP_STORE;
            }
        }

        [[nodiscard]] auto HashCombine(u64 seed, u64 value) noexcept -> u64 {
            constexpr u64 kPrime = 1099511628211ULL;
            return (seed ^ value) * kPrime;
        }

        [[nodiscard]] auto HashAttachments(const TVector<VkFormat>& colors, VkFormat depthFormat,
            ERhiPrimitiveTopology topology) -> u64 {
            constexpr u64 kOffset = 1469598103934665603ULL;
            u64           hash    = kOffset;
            for (VkFormat format : colors) {
                hash = HashCombine(hash, static_cast<u64>(format));
            }
            hash = HashCombine(hash, static_cast<u64>(depthFormat));
            hash = HashCombine(hash, static_cast<u64>(topology));
            return hash;
        }

        auto GetRenderTargetViewHandle(FRhiRenderTargetView* view) noexcept -> VkImageView {
            if (!view) {
                return VK_NULL_HANDLE;
            }
            auto* vkView = static_cast<FRhiVulkanRenderTargetView*>(view);
            return vkView ? vkView->GetImageView() : VK_NULL_HANDLE;
        }

        auto GetDepthStencilViewHandle(FRhiDepthStencilView* view) noexcept -> VkImageView {
            if (!view) {
                return VK_NULL_HANDLE;
            }
            auto* vkView = static_cast<FRhiVulkanDepthStencilView*>(view);
            return vkView ? vkView->GetImageView() : VK_NULL_HANDLE;
        }

        [[nodiscard]] auto BuildRenderPass(VkDevice device, const FRhiRenderPassDesc& desc,
            VkRenderPass& outRenderPass, VkFramebuffer& outFramebuffer, VkExtent2D& outExtent,
            TVector<VkClearValue>& clears, TVector<VkImageView>& attachments) -> bool {
            if (!device) {
                return false;
            }

            const u32   colorCount = desc.mColorAttachmentCount;
            VkImageView depthView  = VK_NULL_HANDLE;

            attachments.Clear();
            clears.Clear();

            TVector<VkAttachmentDescription> attachmentDescs;
            attachmentDescs.Reserve(colorCount + 1);

            TVector<VkAttachmentReference> colorRefs;
            colorRefs.Reserve(colorCount);

            for (u32 i = 0; i < colorCount; ++i) {
                const auto& color = desc.mColorAttachments[i];
                VkImageView view  = GetRenderTargetViewHandle(color.mView);
                if (!view) {
                    continue;
                }
                auto* texture = color.mView->GetTexture();
                if (!texture) {
                    continue;
                }
                VkFormat format = Vulkan::Detail::ToVkFormat(texture->GetDesc().mFormat);

                VkAttachmentDescription attachment{};
                attachment.format         = format;
                attachment.samples        = VK_SAMPLE_COUNT_1_BIT;
                attachment.loadOp         = ToVkLoadOp(color.mLoadOp);
                attachment.storeOp        = ToVkStoreOp(color.mStoreOp);
                attachment.stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
                attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
                attachment.initialLayout  = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
                attachment.finalLayout    = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
                attachmentDescs.PushBack(attachment);

                VkAttachmentReference ref{};
                ref.attachment = static_cast<u32>(attachments.Size());
                ref.layout     = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
                colorRefs.PushBack(ref);

                VkClearValue clear{};
                clear.color.float32[0] = color.mClearColor.mR;
                clear.color.float32[1] = color.mClearColor.mG;
                clear.color.float32[2] = color.mClearColor.mB;
                clear.color.float32[3] = color.mClearColor.mA;
                clears.PushBack(clear);

                attachments.PushBack(view);
                if (outExtent.width == 0) {
                    outExtent.width  = texture->GetDesc().mWidth;
                    outExtent.height = texture->GetDesc().mHeight;
                }
            }

            VkAttachmentReference depthRef{};
            if (desc.mDepthStencilAttachment && desc.mDepthStencilAttachment->mView) {
                auto* dsv = desc.mDepthStencilAttachment->mView;
                depthView = GetDepthStencilViewHandle(dsv);
                if (depthView) {
                    auto*    texture     = dsv->GetTexture();
                    VkFormat depthFormat = Vulkan::Detail::ToVkFormat(
                        texture ? texture->GetDesc().mFormat : ERhiFormat::D32Float);

                    VkAttachmentDescription attachment{};
                    attachment.format  = depthFormat;
                    attachment.samples = VK_SAMPLE_COUNT_1_BIT;
                    attachment.loadOp  = ToVkLoadOp(desc.mDepthStencilAttachment->mDepthLoadOp);
                    attachment.storeOp = ToVkStoreOp(desc.mDepthStencilAttachment->mDepthStoreOp);
                    attachment.stencilLoadOp =
                        ToVkLoadOp(desc.mDepthStencilAttachment->mStencilLoadOp);
                    attachment.stencilStoreOp =
                        ToVkStoreOp(desc.mDepthStencilAttachment->mStencilStoreOp);
                    attachment.initialLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
                    attachment.finalLayout   = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
                    attachmentDescs.PushBack(attachment);

                    depthRef.attachment = static_cast<u32>(attachments.Size());
                    depthRef.layout     = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

                    VkClearValue clear{};
                    clear.depthStencil.depth =
                        desc.mDepthStencilAttachment->mClearDepthStencil.mDepth;
                    clear.depthStencil.stencil =
                        desc.mDepthStencilAttachment->mClearDepthStencil.mStencil;
                    clears.PushBack(clear);

                    attachments.PushBack(depthView);
                    if (outExtent.width == 0 && texture) {
                        outExtent.width  = texture->GetDesc().mWidth;
                        outExtent.height = texture->GetDesc().mHeight;
                    }
                }
            }

            VkSubpassDescription subpass{};
            subpass.pipelineBindPoint       = VK_PIPELINE_BIND_POINT_GRAPHICS;
            subpass.colorAttachmentCount    = static_cast<u32>(colorRefs.Size());
            subpass.pColorAttachments       = colorRefs.IsEmpty() ? nullptr : colorRefs.Data();
            subpass.pDepthStencilAttachment = (depthView != VK_NULL_HANDLE) ? &depthRef : nullptr;

            VkRenderPassCreateInfo rpInfo{};
            rpInfo.sType           = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
            rpInfo.attachmentCount = static_cast<u32>(attachmentDescs.Size());
            rpInfo.pAttachments    = attachmentDescs.Data();
            rpInfo.subpassCount    = 1;
            rpInfo.pSubpasses      = &subpass;

            if (vkCreateRenderPass(device, &rpInfo, nullptr, &outRenderPass) != VK_SUCCESS) {
                return false;
            }

            VkFramebufferCreateInfo fbInfo{};
            fbInfo.sType           = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
            fbInfo.renderPass      = outRenderPass;
            fbInfo.attachmentCount = static_cast<u32>(attachments.Size());
            fbInfo.pAttachments    = attachments.Data();
            fbInfo.width           = outExtent.width;
            fbInfo.height          = outExtent.height;
            fbInfo.layers          = 1;

            if (vkCreateFramebuffer(device, &fbInfo, nullptr, &outFramebuffer) != VK_SUCCESS) {
                vkDestroyRenderPass(device, outRenderPass, nullptr);
                outRenderPass = VK_NULL_HANDLE;
                return false;
            }

            return true;
        }
    } // namespace
    struct FRhiVulkanCommandPool::FState {
        VkDevice      mDevice      = VK_NULL_HANDLE;
        VkCommandPool mPool        = VK_NULL_HANDLE;
        u32           mQueueFamily = 0U;
    };

    FRhiVulkanCommandPool::FRhiVulkanCommandPool(
        const FRhiCommandPoolDesc& desc, VkDevice device, u32 queueFamilyIndex, bool transient)
        : FRhiCommandPool(desc) {
        mState = new FState{};
        if (mState) {
            mState->mDevice      = device;
            mState->mQueueFamily = queueFamilyIndex;
            VkCommandPoolCreateInfo info{};
            info.sType            = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
            info.queueFamilyIndex = queueFamilyIndex;
            info.flags            = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
            if (transient) {
                info.flags |= VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
            }
            vkCreateCommandPool(device, &info, nullptr, &mState->mPool);
        }
    }

    FRhiVulkanCommandPool::~FRhiVulkanCommandPool() {
        if (mState && mState->mDevice && mState->mPool) {
            vkDestroyCommandPool(mState->mDevice, mState->mPool, nullptr);
        }
        delete mState;
        mState = nullptr;
    }

    auto FRhiVulkanCommandPool::GetNativePool() const noexcept -> VkCommandPool {
        return mState ? mState->mPool : VK_NULL_HANDLE;
    }

    auto FRhiVulkanCommandPool::GetQueueFamilyIndex() const noexcept -> u32 {
        return mState ? mState->mQueueFamily : 0U;
    }

    void FRhiVulkanCommandPool::Reset() {
        if (mState && mState->mDevice && mState->mPool) {
            vkResetCommandPool(mState->mDevice, mState->mPool, 0);
        }
    }

    struct FRhiVulkanCommandList::FState {
        VkDevice                    mDevice = VK_NULL_HANDLE;
        VkCommandPool               mPool   = VK_NULL_HANDLE;
        VkCommandBuffer             mBuffer = VK_NULL_HANDLE;
        VkCommandBufferLevel        mLevel  = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        TVector<FRhiVulkanTexture*> mTouchedTextures;
    };

    FRhiVulkanCommandList::FRhiVulkanCommandList(const FRhiCommandListDesc& desc)
        : FRhiCommandList(desc) {
        // TODO: (Require Refactor, Manual, Unidentified) Potential memory leak by allocating cmd
        // bufs
        // TODO: (Require Refactor, Manual, CodeStyle) DO NOT USE NEW
        mState         = new FState{};
        mState->mLevel = (desc.mListType == ERhiCommandListType::Bundle)
            ? VK_COMMAND_BUFFER_LEVEL_SECONDARY
            : VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    }

    FRhiVulkanCommandList::~FRhiVulkanCommandList() {
        if (mState && mState->mDevice && mState->mPool && mState->mBuffer) {
            vkFreeCommandBuffers(mState->mDevice, mState->mPool, 1, &mState->mBuffer);
        }
        delete mState;
        mState = nullptr;
    }

    auto FRhiVulkanCommandList::GetNativeCommandBuffer() const noexcept -> VkCommandBuffer {
        return (mState != nullptr) ? mState->mBuffer : VK_NULL_HANDLE;
    }

    void FRhiVulkanCommandList::SetNativeCommandBuffer(VkCommandBuffer buffer) {
        if (!mState) {
            return;
        }
        mState->mBuffer = buffer;
    }

    void FRhiVulkanCommandList::Reset(FRhiCommandPool* pool) {
        auto* vkPool = static_cast<FRhiVulkanCommandPool*>(pool);
        if (!mState || !vkPool) {
            return;
        }
        mState->mTouchedTextures.Clear();
        mState->mPool   = vkPool->GetNativePool();
        mState->mDevice = static_cast<FRhiVulkanDevice*>(RHIGetDevice())->GetNativeDevice();
        if (!mState->mBuffer) {
            VkCommandBufferAllocateInfo info{};
            info.sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
            info.commandPool        = mState->mPool;
            info.level              = mState->mLevel;
            info.commandBufferCount = 1;
            vkAllocateCommandBuffers(mState->mDevice, &info, &mState->mBuffer);
        } else {
            vkResetCommandBuffer(mState->mBuffer, 0);
        }
    }

    void FRhiVulkanCommandList::Close() {
        if (mState && mState->mBuffer) {
            vkEndCommandBuffer(mState->mBuffer);
        }
    }

    void FRhiVulkanCommandList::AddTouchedTexture(FRhiVulkanTexture* texture) {
        if (!mState || texture == nullptr) {
            return;
        }
        for (auto* existing : mState->mTouchedTextures) {
            if (existing == texture) {
                return;
            }
        }
        mState->mTouchedTextures.PushBack(texture);
    }

    auto FRhiVulkanCommandList::GetTouchedTextures() const noexcept
        -> const TVector<FRhiVulkanTexture*>& {
        static TVector<FRhiVulkanTexture*> sEmpty;
        return mState ? mState->mTouchedTextures : sEmpty;
    }

    void FRhiVulkanCommandList::ClearTouchedTextures() {
        if (mState) {
            mState->mTouchedTextures.Clear();
        }
    }
    struct FRhiVulkanCommandContext::FState {
        VkDevice                           mDevice                       = VK_NULL_HANDLE;
        VkCommandBuffer                    mCmd                          = VK_NULL_HANDLE;
        FRhiVulkanGraphicsPipeline*        mGraphicsPipeline             = nullptr;
        FRhiVulkanComputePipeline*         mComputePipeline              = nullptr;
        VkPipeline                         mBoundPipeline                = VK_NULL_HANDLE;
        bool                               mUseComputePipeline           = false;
        bool                               mInRenderPass                 = false;
        bool                               mDynamicRendering             = false;
        bool                               mSupportsDynamicRendering     = false;
        bool                               mSupportsSync2                = false;
        bool                               mSupportsExtendedDynamicState = false;
        ERhiPrimitiveTopology              mTopology         = ERhiPrimitiveTopology::TriangleList;
        u32                                mQueueFamilyIndex = 0U;
        u64                                mAttachmentHash   = 0ULL;
        VkExtent2D                         mRenderExtent{};
        TVector<VkRenderingAttachmentInfo> mColorAttachments;
        VkRenderingAttachmentInfo          mDepthAttachment{};
        VkRenderingInfo                    mRenderingInfo{};
        VkPipelineRenderingCreateInfo      mPipelineRenderingInfo{};
        VkRenderPass                       mLegacyRenderPass  = VK_NULL_HANDLE;
        VkFramebuffer                      mLegacyFramebuffer = VK_NULL_HANDLE;
        TVector<VkClearValue>              mClearValues;
        TVector<VkImageView>               mLegacyAttachments;
        TVector<VkFormat>                  mColorFormats;
        VkFormat                           mDepthFormat = VK_FORMAT_UNDEFINED;
        FRhiVulkanDevice*                  mOwner       = nullptr;
    };

    FRhiVulkanCommandContext::FRhiVulkanCommandContext(const FRhiCommandContextDesc& desc,
        VkDevice device, FRhiVulkanDevice* owner, FRhiCommandPoolRef pool,
        FRhiCommandListRef commandList)
        : FRhiCommandContext(desc), mPool(Move(pool)), mCommandList(Move(commandList)) {
        mState = new FState{};
        if (mState) {
            mState->mDevice = device;
            mState->mOwner  = owner;
            if (owner) {
                mState->mSupportsDynamicRendering     = owner->SupportsDynamicRendering();
                mState->mSupportsSync2                = owner->SupportsSynchronization2();
                mState->mSupportsExtendedDynamicState = owner->SupportsExtendedDynamicState();
                mState->mQueueFamilyIndex             = owner->GetQueueFamilyIndex(desc.mQueueType);
            }
        }
    }

    FRhiVulkanCommandContext::~FRhiVulkanCommandContext() {
        delete mState;
        mState = nullptr;
    }

    void FRhiVulkanCommandContext::Begin() {
        if (!mState || !mState->mDevice) {
            return;
        }
        if (mPool) {
            mPool->Reset();
        }
        auto* list = static_cast<FRhiVulkanCommandList*>(mCommandList.Get());
        if (list) {
            list->Reset(mPool.Get());
            mState->mCmd = list->GetNativeCommandBuffer();
        }
        if (!mState->mCmd) {
            return;
        }

        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        vkBeginCommandBuffer(mState->mCmd, &beginInfo);

        mState->mGraphicsPipeline   = nullptr;
        mState->mComputePipeline    = nullptr;
        mState->mBoundPipeline      = VK_NULL_HANDLE;
        mState->mUseComputePipeline = false;
        mState->mInRenderPass       = false;
        mState->mTopology           = ERhiPrimitiveTopology::TriangleList;
        mState->mAttachmentHash     = 0ULL;
    }

    void FRhiVulkanCommandContext::End() {
        if (!mState || !mState->mCmd) {
            return;
        }
        if (mState->mInRenderPass) {
            RHIEndRenderPass();
        }
        vkEndCommandBuffer(mState->mCmd);
    }

    auto FRhiVulkanCommandContext::GetCommandList() const noexcept -> FRhiCommandList* {
        return mCommandList.Get();
    }

    void FRhiVulkanCommandContext::RHIUpdateDynamicBufferDiscard(
        FRhiBuffer* buffer, const void* data, u64 sizeBytes, u64 offsetBytes) {
        // Minimal implementation for now: go through the generic buffer lock API.
        // This keeps the interface functional for dynamic constant buffer updates.
        if (buffer == nullptr || data == nullptr || sizeBytes == 0ULL) {
            return;
        }
        auto lock = buffer->Lock(offsetBytes, sizeBytes, ERhiBufferLockMode::WriteDiscard);
        if (!lock.IsValid()) {
            return;
        }
        Core::Platform::Generic::Memcpy(lock.mData, data, static_cast<usize>(sizeBytes));
        buffer->Unlock(lock);
    }

    auto FRhiVulkanCommandContext::GetNativeCommandBuffer() const noexcept -> VkCommandBuffer {
        return mState ? mState->mCmd : VK_NULL_HANDLE;
    }

    void FRhiVulkanCommandContext::RHISetGraphicsPipeline(FRhiPipeline* pipeline) {
        if (!mState || !mState->mCmd) {
            return;
        }

        FRhiVulkanGraphicsPipeline* graphicsPipeline = nullptr;
        if (pipeline != nullptr && pipeline->IsGraphics()) {
            graphicsPipeline = static_cast<FRhiVulkanGraphicsPipeline*>(pipeline);
        }

        mState->mGraphicsPipeline   = graphicsPipeline;
        mState->mUseComputePipeline = false;

        if (mState->mSupportsExtendedDynamicState) {
            vkCmdSetPrimitiveTopology(
                mState->mCmd, Vulkan::Detail::ToVkPrimitiveTopology(mState->mTopology));
        }

        if (mState->mGraphicsPipeline) {
            const VkPipeline pipelineHandle = mState->mGraphicsPipeline->GetOrCreatePipeline(
                mState->mAttachmentHash, mState->mLegacyRenderPass,
                mState->mDynamicRendering ? &mState->mPipelineRenderingInfo : nullptr,
                Vulkan::Detail::ToVkPrimitiveTopology(mState->mTopology));
            if (pipelineHandle != VK_NULL_HANDLE && pipelineHandle != mState->mBoundPipeline) {
                vkCmdBindPipeline(mState->mCmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineHandle);
                mState->mBoundPipeline = pipelineHandle;
            }
        }
    }

    void FRhiVulkanCommandContext::RHISetComputePipeline(FRhiPipeline* pipeline) {
        if (!mState || !mState->mCmd) {
            return;
        }

        FRhiVulkanComputePipeline* computePipeline = nullptr;
        if (pipeline != nullptr && !pipeline->IsGraphics()) {
            computePipeline = static_cast<FRhiVulkanComputePipeline*>(pipeline);
        }

        mState->mComputePipeline    = computePipeline;
        mState->mUseComputePipeline = true;

        if (computePipeline) {
            const VkPipeline pipelineHandle = computePipeline->GetNativePipeline();
            if (pipelineHandle != VK_NULL_HANDLE && pipelineHandle != mState->mBoundPipeline) {
                vkCmdBindPipeline(mState->mCmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipelineHandle);
                mState->mBoundPipeline = pipelineHandle;
            }
        }
    }

    void FRhiVulkanCommandContext::RHISetPrimitiveTopology(ERhiPrimitiveTopology topology) {
        if (!mState || !mState->mCmd) {
            return;
        }
        mState->mTopology = topology;
        if (mState->mSupportsExtendedDynamicState) {
            vkCmdSetPrimitiveTopology(
                mState->mCmd, Vulkan::Detail::ToVkPrimitiveTopology(topology));
        } else if (!mState->mUseComputePipeline && mState->mGraphicsPipeline) {
            const VkPipeline pipelineHandle = mState->mGraphicsPipeline->GetOrCreatePipeline(
                mState->mAttachmentHash, mState->mLegacyRenderPass,
                mState->mDynamicRendering ? &mState->mPipelineRenderingInfo : nullptr,
                Vulkan::Detail::ToVkPrimitiveTopology(mState->mTopology));
            if (pipelineHandle != VK_NULL_HANDLE && pipelineHandle != mState->mBoundPipeline) {
                vkCmdBindPipeline(mState->mCmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineHandle);
                mState->mBoundPipeline = pipelineHandle;
            }
        }
    }

    void FRhiVulkanCommandContext::RHISetVertexBuffer(u32 slot, const FRhiVertexBufferView& view) {
        if (!mState || !mState->mCmd) {
            return;
        }
        VkBuffer     buffer = VK_NULL_HANDLE;
        VkDeviceSize offset = static_cast<VkDeviceSize>(view.mOffsetBytes);
        if (view.mBuffer) {
            auto* vkBuffer = static_cast<FRhiVulkanBuffer*>(view.mBuffer);
            buffer         = vkBuffer ? vkBuffer->GetNativeBuffer() : VK_NULL_HANDLE;
        }
        vkCmdBindVertexBuffers(mState->mCmd, slot, 1, &buffer, &offset);
    }

    void FRhiVulkanCommandContext::RHISetIndexBuffer(const FRhiIndexBufferView& view) {
        if (!mState || !mState->mCmd) {
            return;
        }
        VkBuffer buffer = VK_NULL_HANDLE;
        if (view.mBuffer) {
            auto* vkBuffer = static_cast<FRhiVulkanBuffer*>(view.mBuffer);
            buffer         = vkBuffer ? vkBuffer->GetNativeBuffer() : VK_NULL_HANDLE;
        }
        if (buffer != VK_NULL_HANDLE) {
            vkCmdBindIndexBuffer(mState->mCmd, buffer, static_cast<VkDeviceSize>(view.mOffsetBytes),
                ToVkIndexType(view.mIndexType));
        }
    }

    void FRhiVulkanCommandContext::RHISetViewport(const FRhiViewportRect& viewport) {
        if (!mState || !mState->mCmd) {
            return;
        }
        VkViewport vp{};
        vp.x        = viewport.mX;
        vp.y        = viewport.mY;
        vp.width    = viewport.mWidth;
        vp.height   = viewport.mHeight;
        vp.minDepth = viewport.mMinDepth;
        vp.maxDepth = viewport.mMaxDepth;
        vkCmdSetViewport(mState->mCmd, 0, 1, &vp);
    }

    void FRhiVulkanCommandContext::RHISetScissor(const FRhiScissorRect& scissor) {
        if (!mState || !mState->mCmd) {
            return;
        }
        VkRect2D rect{};
        rect.offset.x      = scissor.mX;
        rect.offset.y      = scissor.mY;
        rect.extent.width  = scissor.mWidth;
        rect.extent.height = scissor.mHeight;
        vkCmdSetScissor(mState->mCmd, 0, 1, &rect);
    }
    void FRhiVulkanCommandContext::RHISetRenderTargets(
        u32 colorTargetCount, FRhiTexture* const* colorTargets, FRhiTexture* depthTarget) {
        if (!mState || !mState->mCmd) {
            return;
        }
        auto* list = static_cast<FRhiVulkanCommandList*>(mCommandList.Get());

        TVector<FRhiRenderPassColorAttachment> colorAttachments;
        colorAttachments.Reserve(colorTargetCount);

        for (u32 i = 0; i < colorTargetCount; ++i) {
            FRhiTexture* texture = colorTargets ? colorTargets[i] : nullptr;
            if (!texture) {
                continue;
            }
            if (list) {
                auto* vkTex = static_cast<FRhiVulkanTexture*>(texture);
                if (vkTex) {
                    list->AddTouchedTexture(vkTex);
                }
            }
            FRhiRenderTargetViewDesc rtvDesc{};
            rtvDesc.mTexture = texture;
            auto rtvRef =
                static_cast<FRhiVulkanDevice*>(RHIGetDevice())->CreateRenderTargetView(rtvDesc);
            if (rtvRef) {
                FRhiRenderPassColorAttachment attachment{};
                attachment.mView       = rtvRef.Get();
                attachment.mLoadOp     = ERhiLoadOp::Load;
                attachment.mStoreOp    = ERhiStoreOp::Store;
                attachment.mClearColor = {};
                colorAttachments.PushBack(attachment);
            }
        }

        FRhiRenderPassDepthStencilAttachment depthAttachment{};
        FRhiDepthStencilViewRef              depthView;
        if (depthTarget) {
            if (list) {
                auto* vkTex = static_cast<FRhiVulkanTexture*>(depthTarget);
                if (vkTex) {
                    list->AddTouchedTexture(vkTex);
                }
            }
            FRhiDepthStencilViewDesc dsvDesc{};
            dsvDesc.mTexture = depthTarget;
            depthView =
                static_cast<FRhiVulkanDevice*>(RHIGetDevice())->CreateDepthStencilView(dsvDesc);
            if (depthView) {
                depthAttachment.mView           = depthView.Get();
                depthAttachment.mDepthLoadOp    = ERhiLoadOp::Load;
                depthAttachment.mDepthStoreOp   = ERhiStoreOp::Store;
                depthAttachment.mStencilLoadOp  = ERhiLoadOp::Load;
                depthAttachment.mStencilStoreOp = ERhiStoreOp::Store;
            }
        }

        FRhiRenderPassDesc passDesc{};
        passDesc.mColorAttachmentCount = static_cast<u32>(colorAttachments.Size());
        passDesc.mColorAttachments = colorAttachments.IsEmpty() ? nullptr : colorAttachments.Data();
        passDesc.mDepthStencilAttachment = depthView ? &depthAttachment : nullptr;
        RHIBeginRenderPass(passDesc);
    }

    void FRhiVulkanCommandContext::RHIBeginRenderPass(const FRhiRenderPassDesc& desc) {
        if (!mState || !mState->mCmd) {
            return;
        }
        if (mState->mInRenderPass) {
            RHIEndRenderPass();
        }

        mState->mColorAttachments.Clear();
        mState->mClearValues.Clear();
        mState->mColorFormats.Clear();
        mState->mDepthFormat  = VK_FORMAT_UNDEFINED;
        mState->mRenderExtent = {};

        const u32 colorCount = desc.mColorAttachmentCount;
        mState->mColorAttachments.Reserve(colorCount);
        mState->mColorFormats.Reserve(colorCount);
        auto* list = static_cast<FRhiVulkanCommandList*>(mCommandList.Get());

        for (u32 i = 0; i < colorCount; ++i) {
            const auto& color = desc.mColorAttachments[i];
            VkImageView view  = GetRenderTargetViewHandle(color.mView);
            if (!view || !color.mView || !color.mView->GetTexture()) {
                continue;
            }

            auto* texture = color.mView->GetTexture();
            if (list) {
                auto* vkTex = static_cast<FRhiVulkanTexture*>(texture);
                if (vkTex) {
                    list->AddTouchedTexture(vkTex);
                }
            }
            const VkFormat format = Vulkan::Detail::ToVkFormat(texture->GetDesc().mFormat);
            mState->mColorFormats.PushBack(format);

            VkRenderingAttachmentInfo attachment{};
            attachment.sType                       = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
            attachment.imageView                   = view;
            attachment.imageLayout                 = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            attachment.loadOp                      = ToVkLoadOp(color.mLoadOp);
            attachment.storeOp                     = ToVkStoreOp(color.mStoreOp);
            attachment.clearValue.color.float32[0] = color.mClearColor.mR;
            attachment.clearValue.color.float32[1] = color.mClearColor.mG;
            attachment.clearValue.color.float32[2] = color.mClearColor.mB;
            attachment.clearValue.color.float32[3] = color.mClearColor.mA;

            mState->mColorAttachments.PushBack(attachment);

            VkClearValue clear{};
            clear.color.float32[0] = color.mClearColor.mR;
            clear.color.float32[1] = color.mClearColor.mG;
            clear.color.float32[2] = color.mClearColor.mB;
            clear.color.float32[3] = color.mClearColor.mA;
            mState->mClearValues.PushBack(clear);

            if (mState->mRenderExtent.width == 0) {
                mState->mRenderExtent.width  = texture->GetDesc().mWidth;
                mState->mRenderExtent.height = texture->GetDesc().mHeight;
            }
        }

        bool hasDepth = false;
        if (desc.mDepthStencilAttachment && desc.mDepthStencilAttachment->mView) {
            auto*       dsv  = desc.mDepthStencilAttachment->mView;
            VkImageView view = GetDepthStencilViewHandle(dsv);
            if (view && dsv->GetTexture()) {
                auto* texture = dsv->GetTexture();
                if (list) {
                    auto* vkTex = static_cast<FRhiVulkanTexture*>(texture);
                    if (vkTex) {
                        list->AddTouchedTexture(vkTex);
                    }
                }
                mState->mDepthFormat = Vulkan::Detail::ToVkFormat(texture->GetDesc().mFormat);
                hasDepth             = true;

                mState->mDepthAttachment           = {};
                mState->mDepthAttachment.sType     = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
                mState->mDepthAttachment.imageView = view;
                mState->mDepthAttachment.imageLayout =
                    VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
                mState->mDepthAttachment.loadOp =
                    ToVkLoadOp(desc.mDepthStencilAttachment->mDepthLoadOp);
                mState->mDepthAttachment.storeOp =
                    ToVkStoreOp(desc.mDepthStencilAttachment->mDepthStoreOp);
                mState->mDepthAttachment.clearValue.depthStencil.depth =
                    desc.mDepthStencilAttachment->mClearDepthStencil.mDepth;
                mState->mDepthAttachment.clearValue.depthStencil.stencil =
                    desc.mDepthStencilAttachment->mClearDepthStencil.mStencil;

                VkClearValue clear{};
                clear.depthStencil.depth = desc.mDepthStencilAttachment->mClearDepthStencil.mDepth;
                clear.depthStencil.stencil =
                    desc.mDepthStencilAttachment->mClearDepthStencil.mStencil;
                mState->mClearValues.PushBack(clear);

                if (mState->mRenderExtent.width == 0) {
                    mState->mRenderExtent.width  = texture->GetDesc().mWidth;
                    mState->mRenderExtent.height = texture->GetDesc().mHeight;
                }
            }
        }

        mState->mAttachmentHash =
            HashAttachments(mState->mColorFormats, mState->mDepthFormat, mState->mTopology);

        if (mState->mSupportsDynamicRendering) {
            mState->mDynamicRendering                = true;
            mState->mRenderingInfo                   = {};
            mState->mRenderingInfo.sType             = VK_STRUCTURE_TYPE_RENDERING_INFO;
            mState->mRenderingInfo.renderArea.offset = { 0, 0 };
            mState->mRenderingInfo.renderArea.extent = mState->mRenderExtent;
            mState->mRenderingInfo.layerCount        = 1;
            mState->mRenderingInfo.colorAttachmentCount =
                static_cast<u32>(mState->mColorAttachments.Size());
            mState->mRenderingInfo.pColorAttachments = mState->mColorAttachments.Data();
            mState->mRenderingInfo.pDepthAttachment =
                hasDepth ? &mState->mDepthAttachment : nullptr;

            mState->mPipelineRenderingInfo       = {};
            mState->mPipelineRenderingInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
            mState->mPipelineRenderingInfo.colorAttachmentCount =
                static_cast<u32>(mState->mColorFormats.Size());
            mState->mPipelineRenderingInfo.pColorAttachmentFormats = mState->mColorFormats.Data();
            mState->mPipelineRenderingInfo.depthAttachmentFormat   = mState->mDepthFormat;
            mState->mPipelineRenderingInfo.stencilAttachmentFormat = mState->mDepthFormat;

            vkCmdBeginRendering(mState->mCmd, &mState->mRenderingInfo);
        } else {
            mState->mDynamicRendering  = false;
            mState->mLegacyRenderPass  = VK_NULL_HANDLE;
            mState->mLegacyFramebuffer = VK_NULL_HANDLE;
            if (!BuildRenderPass(mState->mDevice, desc, mState->mLegacyRenderPass,
                    mState->mLegacyFramebuffer, mState->mRenderExtent, mState->mClearValues,
                    mState->mLegacyAttachments)) {
                return;
            }

            VkRenderPassBeginInfo beginInfo{};
            beginInfo.sType             = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
            beginInfo.renderPass        = mState->mLegacyRenderPass;
            beginInfo.framebuffer       = mState->mLegacyFramebuffer;
            beginInfo.renderArea.offset = { 0, 0 };
            beginInfo.renderArea.extent = mState->mRenderExtent;
            beginInfo.clearValueCount   = static_cast<u32>(mState->mClearValues.Size());
            beginInfo.pClearValues      = mState->mClearValues.Data();
            vkCmdBeginRenderPass(mState->mCmd, &beginInfo, VK_SUBPASS_CONTENTS_INLINE);
        }

        mState->mInRenderPass = true;

        if (mState->mGraphicsPipeline) {
            const VkPipeline pipelineHandle = mState->mGraphicsPipeline->GetOrCreatePipeline(
                mState->mAttachmentHash, mState->mLegacyRenderPass,
                mState->mDynamicRendering ? &mState->mPipelineRenderingInfo : nullptr,
                Vulkan::Detail::ToVkPrimitiveTopology(mState->mTopology));
            if (pipelineHandle != VK_NULL_HANDLE && pipelineHandle != mState->mBoundPipeline) {
                vkCmdBindPipeline(mState->mCmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineHandle);
                mState->mBoundPipeline = pipelineHandle;
            }
        }
    }

    void FRhiVulkanCommandContext::RHIEndRenderPass() {
        if (!mState || !mState->mCmd || !mState->mInRenderPass) {
            return;
        }
        if (mState->mDynamicRendering) {
            vkCmdEndRendering(mState->mCmd);
        } else {
            vkCmdEndRenderPass(mState->mCmd);
            if (mState->mLegacyFramebuffer) {
                vkDestroyFramebuffer(mState->mDevice, mState->mLegacyFramebuffer, nullptr);
                mState->mLegacyFramebuffer = VK_NULL_HANDLE;
            }
            if (mState->mLegacyRenderPass) {
                vkDestroyRenderPass(mState->mDevice, mState->mLegacyRenderPass, nullptr);
                mState->mLegacyRenderPass = VK_NULL_HANDLE;
            }
        }
        mState->mInRenderPass = false;
    }

    void FRhiVulkanCommandContext::RHIBeginTransition(const FRhiTransitionCreateInfo& info) {
        if (!mState || !mState->mCmd || info.mTransitionCount == 0U
            || info.mTransitions == nullptr) {
            return;
        }
        if (mState->mOwner == nullptr) {
            return;
        }

        const bool crossQueue = (info.mSrcQueue != info.mDstQueue);
        if (crossQueue) {
            Core::Utility::Assert(info.mTransition != nullptr, TEXT("RHI.Vulkan"),
                "Cross-queue transition requires a valid FRhiTransition.");
            if (info.mTransition == nullptr) {
                return;
            }
        }
        auto addTouchedTexture = [this](FRhiTexture* texture) {
            auto* list  = static_cast<FRhiVulkanCommandList*>(mCommandList.Get());
            auto* vkTex = static_cast<FRhiVulkanTexture*>(texture);
            if (list && vkTex) {
                list->AddTouchedTexture(vkTex);
            }
        };
        const u32 srcFamily = crossQueue ? mState->mOwner->GetQueueFamilyIndex(info.mSrcQueue)
                                         : VK_QUEUE_FAMILY_IGNORED;
        const u32 dstFamily = crossQueue ? mState->mOwner->GetQueueFamilyIndex(info.mDstQueue)
                                         : VK_QUEUE_FAMILY_IGNORED;

        for (u32 i = 0; i < info.mTransitionCount; ++i) {
            const auto& tr = info.mTransitions[i];
            if (!tr.mResource) {
                continue;
            }

            // Stateless barrier generation based solely on the transition info.
            if (auto* texture = AltinaEngine::CheckedCast<FRhiTexture*>(tr.mResource)) {
                addTouchedTexture(texture);
                auto* vkTex = static_cast<FRhiVulkanTexture*>(texture);
                if (!vkTex) {
                    continue;
                }

                const auto& texDesc = vkTex->GetDesc();
                const bool  isDepth = Vulkan::Detail::IsDepthFormat(texDesc.mFormat);

                const auto  before = Vulkan::Detail::MapResourceState(tr.mBefore, isDepth);
                const auto  after  = Vulkan::Detail::MapResourceState(tr.mAfter, isDepth);

                VkImageSubresourceRange range{};
                range.aspectMask     = Vulkan::Detail::ToVkAspectFlags(texDesc.mFormat);
                range.baseMipLevel   = tr.mTextureRange.mBaseMip;
                range.levelCount     = (tr.mTextureRange.mMipCount == 0U) ? texDesc.mMipLevels
                                                                          : tr.mTextureRange.mMipCount;
                range.baseArrayLayer = tr.mTextureRange.mBaseArrayLayer;
                range.layerCount     = (tr.mTextureRange.mLayerCount == 0U)
                        ? texDesc.mArrayLayers
                        : tr.mTextureRange.mLayerCount;

                if (mState->mSupportsSync2) {
                    VkImageMemoryBarrier2 barrier{};
                    barrier.sType               = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
                    barrier.image               = vkTex->GetNativeImage();
                    barrier.subresourceRange    = range;
                    barrier.oldLayout           = before.mLayout;
                    barrier.newLayout           = crossQueue ? before.mLayout : after.mLayout;
                    barrier.srcQueueFamilyIndex = srcFamily;
                    barrier.dstQueueFamilyIndex = dstFamily;
                    barrier.srcStageMask        = before.mStages;
                    barrier.srcAccessMask       = before.mAccess;
                    barrier.dstStageMask  = crossQueue ? VK_PIPELINE_STAGE_2_NONE : after.mStages;
                    barrier.dstAccessMask = crossQueue ? 0 : after.mAccess;

                    VkDependencyInfo dep{};
                    dep.sType                   = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
                    dep.imageMemoryBarrierCount = 1;
                    dep.pImageMemoryBarriers    = &barrier;
                    vkCmdPipelineBarrier2(mState->mCmd, &dep);
                } else {
                    VkImageMemoryBarrier barrier{};
                    barrier.sType               = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
                    barrier.image               = vkTex->GetNativeImage();
                    barrier.subresourceRange    = range;
                    barrier.oldLayout           = before.mLayout;
                    barrier.newLayout           = crossQueue ? before.mLayout : after.mLayout;
                    barrier.srcQueueFamilyIndex = srcFamily;
                    barrier.dstQueueFamilyIndex = dstFamily;
                    barrier.srcAccessMask       = static_cast<VkAccessFlags>(before.mAccess);
                    barrier.dstAccessMask =
                        crossQueue ? 0 : static_cast<VkAccessFlags>(after.mAccess);

                    vkCmdPipelineBarrier(mState->mCmd,
                        static_cast<VkPipelineStageFlags>(before.mStages),
                        crossQueue ? VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT
                                   : static_cast<VkPipelineStageFlags>(after.mStages),
                        0, 0, nullptr, 0, nullptr, 1, &barrier);
                }
            } else if (auto* buffer = AltinaEngine::CheckedCast<FRhiBuffer*>(tr.mResource)) {
                auto* vkBuf = static_cast<FRhiVulkanBuffer*>(buffer);
                if (!vkBuf) {
                    continue;
                }

                const auto before = Vulkan::Detail::MapResourceState(tr.mBefore, false);
                const auto after  = Vulkan::Detail::MapResourceState(tr.mAfter, false);

                if (mState->mSupportsSync2) {
                    VkBufferMemoryBarrier2 barrier{};
                    barrier.sType               = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2;
                    barrier.buffer              = vkBuf->GetNativeBuffer();
                    barrier.offset              = 0;
                    barrier.size                = VK_WHOLE_SIZE;
                    barrier.srcQueueFamilyIndex = srcFamily;
                    barrier.dstQueueFamilyIndex = dstFamily;
                    barrier.srcStageMask        = before.mStages;
                    barrier.srcAccessMask       = before.mAccess;
                    barrier.dstStageMask  = crossQueue ? VK_PIPELINE_STAGE_2_NONE : after.mStages;
                    barrier.dstAccessMask = crossQueue ? 0 : after.mAccess;

                    VkDependencyInfo dep{};
                    dep.sType                    = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
                    dep.bufferMemoryBarrierCount = 1;
                    dep.pBufferMemoryBarriers    = &barrier;
                    vkCmdPipelineBarrier2(mState->mCmd, &dep);
                } else {
                    VkBufferMemoryBarrier barrier{};
                    barrier.sType               = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
                    barrier.buffer              = vkBuf->GetNativeBuffer();
                    barrier.offset              = 0;
                    barrier.size                = VK_WHOLE_SIZE;
                    barrier.srcQueueFamilyIndex = srcFamily;
                    barrier.dstQueueFamilyIndex = dstFamily;
                    barrier.srcAccessMask       = static_cast<VkAccessFlags>(before.mAccess);
                    barrier.dstAccessMask =
                        crossQueue ? 0 : static_cast<VkAccessFlags>(after.mAccess);

                    vkCmdPipelineBarrier(mState->mCmd,
                        static_cast<VkPipelineStageFlags>(before.mStages),
                        crossQueue ? VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT
                                   : static_cast<VkPipelineStageFlags>(after.mStages),
                        0, 0, nullptr, 1, &barrier, 0, nullptr);
                }
            }
        }
    }

    void FRhiVulkanCommandContext::RHIEndTransition(const FRhiTransitionCreateInfo& info) {
        if (!mState || !mState->mCmd || info.mTransitionCount == 0U
            || info.mTransitions == nullptr) {
            return;
        }
        if (mState->mOwner == nullptr) {
            return;
        }
        if (info.mSrcQueue == info.mDstQueue) {
            return;
        }
        Core::Utility::Assert(info.mTransition != nullptr, TEXT("RHI.Vulkan"),
            "Cross-queue transition requires a valid FRhiTransition.");
        if (info.mTransition == nullptr) {
            return;
        }
        auto addTouchedTexture = [this](FRhiTexture* texture) {
            auto* list  = static_cast<FRhiVulkanCommandList*>(mCommandList.Get());
            auto* vkTex = static_cast<FRhiVulkanTexture*>(texture);
            if (list && vkTex) {
                list->AddTouchedTexture(vkTex);
            }
        };

        const u32 srcFamily = mState->mOwner->GetQueueFamilyIndex(info.mSrcQueue);
        const u32 dstFamily = mState->mOwner->GetQueueFamilyIndex(info.mDstQueue);

        for (u32 i = 0; i < info.mTransitionCount; ++i) {
            const auto& tr = info.mTransitions[i];
            if (!tr.mResource) {
                continue;
            }

            if (auto* texture = AltinaEngine::CheckedCast<FRhiTexture*>(tr.mResource)) {
                addTouchedTexture(texture);
                auto* vkTex = static_cast<FRhiVulkanTexture*>(texture);
                if (!vkTex) {
                    continue;
                }

                const auto& texDesc = vkTex->GetDesc();
                const bool  isDepth = Vulkan::Detail::IsDepthFormat(texDesc.mFormat);
                const auto  before  = Vulkan::Detail::MapResourceState(tr.mBefore, isDepth);
                const auto  after   = Vulkan::Detail::MapResourceState(tr.mAfter, isDepth);

                VkImageSubresourceRange range{};
                range.aspectMask     = Vulkan::Detail::ToVkAspectFlags(texDesc.mFormat);
                range.baseMipLevel   = tr.mTextureRange.mBaseMip;
                range.levelCount     = (tr.mTextureRange.mMipCount == 0U) ? texDesc.mMipLevels
                                                                          : tr.mTextureRange.mMipCount;
                range.baseArrayLayer = tr.mTextureRange.mBaseArrayLayer;
                range.layerCount     = (tr.mTextureRange.mLayerCount == 0U)
                        ? texDesc.mArrayLayers
                        : tr.mTextureRange.mLayerCount;

                if (mState->mSupportsSync2) {
                    VkImageMemoryBarrier2 barrier{};
                    barrier.sType               = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
                    barrier.image               = vkTex->GetNativeImage();
                    barrier.subresourceRange    = range;
                    barrier.oldLayout           = before.mLayout;
                    barrier.newLayout           = after.mLayout;
                    barrier.srcQueueFamilyIndex = srcFamily;
                    barrier.dstQueueFamilyIndex = dstFamily;
                    barrier.srcStageMask        = VK_PIPELINE_STAGE_2_NONE;
                    barrier.srcAccessMask       = 0;
                    barrier.dstStageMask        = after.mStages;
                    barrier.dstAccessMask       = after.mAccess;

                    VkDependencyInfo dep{};
                    dep.sType                   = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
                    dep.imageMemoryBarrierCount = 1;
                    dep.pImageMemoryBarriers    = &barrier;
                    vkCmdPipelineBarrier2(mState->mCmd, &dep);
                } else {
                    VkImageMemoryBarrier barrier{};
                    barrier.sType               = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
                    barrier.image               = vkTex->GetNativeImage();
                    barrier.subresourceRange    = range;
                    barrier.oldLayout           = before.mLayout;
                    barrier.newLayout           = after.mLayout;
                    barrier.srcQueueFamilyIndex = srcFamily;
                    barrier.dstQueueFamilyIndex = dstFamily;
                    barrier.srcAccessMask       = 0;
                    barrier.dstAccessMask       = static_cast<VkAccessFlags>(after.mAccess);

                    vkCmdPipelineBarrier(mState->mCmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                        static_cast<VkPipelineStageFlags>(after.mStages), 0, 0, nullptr, 0, nullptr,
                        1, &barrier);
                }
            } else if (auto* buffer = AltinaEngine::CheckedCast<FRhiBuffer*>(tr.mResource)) {
                auto* vkBuf = static_cast<FRhiVulkanBuffer*>(buffer);
                if (!vkBuf) {
                    continue;
                }

                const auto after = Vulkan::Detail::MapResourceState(tr.mAfter, false);

                if (mState->mSupportsSync2) {
                    VkBufferMemoryBarrier2 barrier{};
                    barrier.sType               = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2;
                    barrier.buffer              = vkBuf->GetNativeBuffer();
                    barrier.offset              = 0;
                    barrier.size                = VK_WHOLE_SIZE;
                    barrier.srcQueueFamilyIndex = srcFamily;
                    barrier.dstQueueFamilyIndex = dstFamily;
                    barrier.srcStageMask        = VK_PIPELINE_STAGE_2_NONE;
                    barrier.srcAccessMask       = 0;
                    barrier.dstStageMask        = after.mStages;
                    barrier.dstAccessMask       = after.mAccess;

                    VkDependencyInfo dep{};
                    dep.sType                    = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
                    dep.bufferMemoryBarrierCount = 1;
                    dep.pBufferMemoryBarriers    = &barrier;
                    vkCmdPipelineBarrier2(mState->mCmd, &dep);
                } else {
                    VkBufferMemoryBarrier barrier{};
                    barrier.sType               = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
                    barrier.buffer              = vkBuf->GetNativeBuffer();
                    barrier.offset              = 0;
                    barrier.size                = VK_WHOLE_SIZE;
                    barrier.srcQueueFamilyIndex = srcFamily;
                    barrier.dstQueueFamilyIndex = dstFamily;
                    barrier.srcAccessMask       = 0;
                    barrier.dstAccessMask       = static_cast<VkAccessFlags>(after.mAccess);

                    vkCmdPipelineBarrier(mState->mCmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                        static_cast<VkPipelineStageFlags>(after.mStages), 0, 0, nullptr, 1,
                        &barrier, 0, nullptr);
                }
            }
        }
    }

    void FRhiVulkanCommandContext::RHIClearColor(
        FRhiTexture* colorTarget, const FRhiClearColor& color) {
        if (!mState || !mState->mCmd || colorTarget == nullptr) {
            return;
        }
        auto* vkTex = static_cast<FRhiVulkanTexture*>(colorTarget);
        if (!vkTex) {
            return;
        }
        auto* list = static_cast<FRhiVulkanCommandList*>(mCommandList.Get());
        if (list) {
            list->AddTouchedTexture(vkTex);
        }

        VkClearColorValue clear{};
        clear.float32[0] = color.mR;
        clear.float32[1] = color.mG;
        clear.float32[2] = color.mB;
        clear.float32[3] = color.mA;

        VkImageSubresourceRange range{};
        range.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
        range.baseMipLevel   = 0;
        range.levelCount     = 1;
        range.baseArrayLayer = 0;
        range.layerCount     = 1;

        vkCmdClearColorImage(
            mState->mCmd, vkTex->GetNativeImage(), VK_IMAGE_LAYOUT_GENERAL, &clear, 1, &range);
    }

    void FRhiVulkanCommandContext::RHISetBindGroup(
        u32 setIndex, FRhiBindGroup* group, const u32* dynamicOffsets, u32 dynamicOffsetCount) {
        if (!mState || !mState->mCmd || group == nullptr) {
            return;
        }

        auto* vkGroup = static_cast<FRhiVulkanBindGroup*>(group);
        if (!vkGroup) {
            return;
        }
        auto* list = static_cast<FRhiVulkanCommandList*>(mCommandList.Get());
        if (list) {
            const auto& desc = group->GetDesc();
            for (const auto& entry : desc.mEntries) {
                if (entry.mTexture != nullptr) {
                    auto* vkTex = static_cast<FRhiVulkanTexture*>(entry.mTexture);
                    if (vkTex) {
                        list->AddTouchedTexture(vkTex);
                    }
                }
            }
        }

        VkPipelineLayout    layout    = VK_NULL_HANDLE;
        VkPipelineBindPoint bindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        if (mState->mUseComputePipeline && mState->mComputePipeline) {
            layout    = mState->mComputePipeline->GetNativeLayout();
            bindPoint = VK_PIPELINE_BIND_POINT_COMPUTE;
        } else if (mState->mGraphicsPipeline) {
            layout    = mState->mGraphicsPipeline->GetNativeLayout();
            bindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        }

        if (!layout) {
            return;
        }

        VkDescriptorSet set = vkGroup->GetDescriptorSet();
        vkCmdBindDescriptorSets(
            mState->mCmd, bindPoint, layout, setIndex, 1, &set, dynamicOffsetCount, dynamicOffsets);
    }

    void FRhiVulkanCommandContext::RHIDraw(
        u32 vertexCount, u32 instanceCount, u32 firstVertex, u32 firstInstance) {
        if (!mState || !mState->mCmd) {
            return;
        }
        vkCmdDraw(mState->mCmd, vertexCount, instanceCount, firstVertex, firstInstance);
    }

    void FRhiVulkanCommandContext::RHIDrawIndexed(
        u32 indexCount, u32 instanceCount, u32 firstIndex, i32 vertexOffset, u32 firstInstance) {
        if (!mState || !mState->mCmd) {
            return;
        }
        vkCmdDrawIndexed(
            mState->mCmd, indexCount, instanceCount, firstIndex, vertexOffset, firstInstance);
    }

    void FRhiVulkanCommandContext::RHIDispatch(u32 groupCountX, u32 groupCountY, u32 groupCountZ) {
        if (!mState || !mState->mCmd) {
            return;
        }
        vkCmdDispatch(mState->mCmd, groupCountX, groupCountY, groupCountZ);
    }
} // namespace AltinaEngine::Rhi
