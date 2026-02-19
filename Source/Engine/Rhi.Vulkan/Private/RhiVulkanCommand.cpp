
#include "RhiVulkan/RhiVulkanCommandContext.h"
#include "RhiVulkan/RhiVulkanCommandList.h"
#include "RhiVulkan/RhiVulkanCommandPool.h"
#include "RhiVulkan/RhiVulkanPipeline.h"
#include "RhiVulkan/RhiVulkanResources.h"
#include "RhiVulkan/RhiVulkanDevice.h"

#include "Rhi/RhiBindGroup.h"
#include "Rhi/RhiPipelineLayout.h"
#include "Rhi/RhiInit.h"

#include "RhiVulkanInternal.h"

using AltinaEngine::Move;
namespace AltinaEngine::Rhi {
    namespace Container = Core::Container;
    using Container::TVector;

#if defined(AE_RHI_VULKAN_AVAILABLE) && AE_RHI_VULKAN_AVAILABLE
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
#endif
    struct FRhiVulkanCommandPool::FState {
#if defined(AE_RHI_VULKAN_AVAILABLE) && AE_RHI_VULKAN_AVAILABLE
        VkDevice      mDevice      = VK_NULL_HANDLE;
        VkCommandPool mPool        = VK_NULL_HANDLE;
        u32           mQueueFamily = 0U;
#endif
    };

    FRhiVulkanCommandPool::FRhiVulkanCommandPool(
        const FRhiCommandPoolDesc& desc, VkDevice device, u32 queueFamilyIndex, bool transient)
        : FRhiCommandPool(desc) {
        mState = new FState{};
#if defined(AE_RHI_VULKAN_AVAILABLE) && AE_RHI_VULKAN_AVAILABLE
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
#else
        (void)device;
        (void)queueFamilyIndex;
        (void)transient;
#endif
    }

    FRhiVulkanCommandPool::~FRhiVulkanCommandPool() {
#if defined(AE_RHI_VULKAN_AVAILABLE) && AE_RHI_VULKAN_AVAILABLE
        if (mState && mState->mDevice && mState->mPool) {
            vkDestroyCommandPool(mState->mDevice, mState->mPool, nullptr);
        }
#endif
        delete mState;
        mState = nullptr;
    }

    auto FRhiVulkanCommandPool::GetNativePool() const noexcept -> VkCommandPool {
#if defined(AE_RHI_VULKAN_AVAILABLE) && AE_RHI_VULKAN_AVAILABLE
        return mState ? mState->mPool : VK_NULL_HANDLE;
#else
        return nullptr;
#endif
    }

    auto FRhiVulkanCommandPool::GetQueueFamilyIndex() const noexcept -> u32 {
        return mState ? mState->mQueueFamily : 0U;
    }

    void FRhiVulkanCommandPool::Reset() {
#if defined(AE_RHI_VULKAN_AVAILABLE) && AE_RHI_VULKAN_AVAILABLE
        if (mState && mState->mDevice && mState->mPool) {
            vkResetCommandPool(mState->mDevice, mState->mPool, 0);
        }
#endif
    }

    struct FRhiVulkanCommandList::FState {
#if defined(AE_RHI_VULKAN_AVAILABLE) && AE_RHI_VULKAN_AVAILABLE
        VkDevice             mDevice = VK_NULL_HANDLE;
        VkCommandPool        mPool   = VK_NULL_HANDLE;
        VkCommandBuffer      mBuffer = VK_NULL_HANDLE;
        VkCommandBufferLevel mLevel  = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
#endif
    };

    FRhiVulkanCommandList::FRhiVulkanCommandList(const FRhiCommandListDesc& desc)
        : FRhiCommandList(desc) {
        mState = new FState{};
#if defined(AE_RHI_VULKAN_AVAILABLE) && AE_RHI_VULKAN_AVAILABLE
        mState->mLevel = (desc.mListType == ERhiCommandListType::Bundle)
            ? VK_COMMAND_BUFFER_LEVEL_SECONDARY
            : VK_COMMAND_BUFFER_LEVEL_PRIMARY;
#endif
    }

    FRhiVulkanCommandList::~FRhiVulkanCommandList() {
#if defined(AE_RHI_VULKAN_AVAILABLE) && AE_RHI_VULKAN_AVAILABLE
        if (mState && mState->mDevice && mState->mPool && mState->mBuffer) {
            vkFreeCommandBuffers(mState->mDevice, mState->mPool, 1, &mState->mBuffer);
        }
#endif
        delete mState;
        mState = nullptr;
    }

    auto FRhiVulkanCommandList::GetNativeCommandBuffer() const noexcept -> VkCommandBuffer {
#if defined(AE_RHI_VULKAN_AVAILABLE) && AE_RHI_VULKAN_AVAILABLE
        return (mState != nullptr) ? mState->mBuffer : VK_NULL_HANDLE;
#else
        return nullptr;
#endif
    }

    void FRhiVulkanCommandList::SetNativeCommandBuffer(VkCommandBuffer buffer) {
#if defined(AE_RHI_VULKAN_AVAILABLE) && AE_RHI_VULKAN_AVAILABLE
        if (!mState) {
            return;
        }
        mState->mBuffer = buffer;
#else
        (void)buffer;
#endif
    }

    void FRhiVulkanCommandList::Reset(FRhiCommandPool* pool) {
#if defined(AE_RHI_VULKAN_AVAILABLE) && AE_RHI_VULKAN_AVAILABLE
        auto* vkPool = static_cast<FRhiVulkanCommandPool*>(pool);
        if (!mState || !vkPool) {
            return;
        }
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
#endif
    }

    void FRhiVulkanCommandList::Close() {
#if defined(AE_RHI_VULKAN_AVAILABLE) && AE_RHI_VULKAN_AVAILABLE
        if (mState && mState->mBuffer) {
            vkEndCommandBuffer(mState->mBuffer);
        }
#endif
    }
    struct FRhiVulkanCommandContext::FState {
#if defined(AE_RHI_VULKAN_AVAILABLE) && AE_RHI_VULKAN_AVAILABLE
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
#endif
    };

    FRhiVulkanCommandContext::FRhiVulkanCommandContext(const FRhiCommandContextDesc& desc,
        VkDevice device, FRhiVulkanDevice* owner, FRhiCommandPoolRef pool,
        FRhiCommandListRef commandList)
        : FRhiCommandContext(desc), mPool(Move(pool)), mCommandList(Move(commandList)) {
        mState = new FState{};
#if defined(AE_RHI_VULKAN_AVAILABLE) && AE_RHI_VULKAN_AVAILABLE
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
#else
        (void)device;
        (void)owner;
#endif
    }

    FRhiVulkanCommandContext::~FRhiVulkanCommandContext() {
        delete mState;
        mState = nullptr;
    }

    void FRhiVulkanCommandContext::Begin() {
#if defined(AE_RHI_VULKAN_AVAILABLE) && AE_RHI_VULKAN_AVAILABLE
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
#endif
    }

    void FRhiVulkanCommandContext::End() {
#if defined(AE_RHI_VULKAN_AVAILABLE) && AE_RHI_VULKAN_AVAILABLE
        if (!mState || !mState->mCmd) {
            return;
        }
        if (mState->mInRenderPass) {
            RHIEndRenderPass();
        }
        vkEndCommandBuffer(mState->mCmd);
#endif
    }

    auto FRhiVulkanCommandContext::GetCommandList() const noexcept -> FRhiCommandList* {
        return mCommandList.Get();
    }

    auto FRhiVulkanCommandContext::GetNativeCommandBuffer() const noexcept -> VkCommandBuffer {
#if defined(AE_RHI_VULKAN_AVAILABLE) && AE_RHI_VULKAN_AVAILABLE
        return mState ? mState->mCmd : VK_NULL_HANDLE;
#else
        return nullptr;
#endif
    }

    void FRhiVulkanCommandContext::RHISetGraphicsPipeline(FRhiPipeline* pipeline) {
#if defined(AE_RHI_VULKAN_AVAILABLE) && AE_RHI_VULKAN_AVAILABLE
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
#else
        (void)pipeline;
#endif
    }

    void FRhiVulkanCommandContext::RHISetComputePipeline(FRhiPipeline* pipeline) {
#if defined(AE_RHI_VULKAN_AVAILABLE) && AE_RHI_VULKAN_AVAILABLE
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
#else
        (void)pipeline;
#endif
    }

    void FRhiVulkanCommandContext::RHISetPrimitiveTopology(ERhiPrimitiveTopology topology) {
#if defined(AE_RHI_VULKAN_AVAILABLE) && AE_RHI_VULKAN_AVAILABLE
        if (!mState || !mState->mCmd) {
            return;
        }
        mState->mTopology = topology;
        if (mState->mSupportsExtendedDynamicState) {
            vkCmdSetPrimitiveTopology(
                mState->mCmd, Vulkan::Detail::ToVkPrimitiveTopology(topology));
        }
#else
        (void)topology;
#endif
    }

    void FRhiVulkanCommandContext::RHISetVertexBuffer(u32 slot, const FRhiVertexBufferView& view) {
#if defined(AE_RHI_VULKAN_AVAILABLE) && AE_RHI_VULKAN_AVAILABLE
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
#else
        (void)slot;
        (void)view;
#endif
    }

    void FRhiVulkanCommandContext::RHISetIndexBuffer(const FRhiIndexBufferView& view) {
#if defined(AE_RHI_VULKAN_AVAILABLE) && AE_RHI_VULKAN_AVAILABLE
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
#else
        (void)view;
#endif
    }

    void FRhiVulkanCommandContext::RHISetViewport(const FRhiViewportRect& viewport) {
#if defined(AE_RHI_VULKAN_AVAILABLE) && AE_RHI_VULKAN_AVAILABLE
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
#else
        (void)viewport;
#endif
    }

    void FRhiVulkanCommandContext::RHISetScissor(const FRhiScissorRect& scissor) {
#if defined(AE_RHI_VULKAN_AVAILABLE) && AE_RHI_VULKAN_AVAILABLE
        if (!mState || !mState->mCmd) {
            return;
        }
        VkRect2D rect{};
        rect.offset.x      = scissor.mX;
        rect.offset.y      = scissor.mY;
        rect.extent.width  = scissor.mWidth;
        rect.extent.height = scissor.mHeight;
        vkCmdSetScissor(mState->mCmd, 0, 1, &rect);
#else
        (void)scissor;
#endif
    }
    void FRhiVulkanCommandContext::RHISetRenderTargets(
        u32 colorTargetCount, FRhiTexture* const* colorTargets, FRhiTexture* depthTarget) {
#if defined(AE_RHI_VULKAN_AVAILABLE) && AE_RHI_VULKAN_AVAILABLE
        if (!mState || !mState->mCmd) {
            return;
        }

        TVector<FRhiRenderPassColorAttachment> colorAttachments;
        colorAttachments.Reserve(colorTargetCount);

        for (u32 i = 0; i < colorTargetCount; ++i) {
            FRhiTexture* texture = colorTargets ? colorTargets[i] : nullptr;
            if (!texture) {
                continue;
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
#else
        (void)colorTargetCount;
        (void)colorTargets;
        (void)depthTarget;
#endif
    }

    void FRhiVulkanCommandContext::RHIBeginRenderPass(const FRhiRenderPassDesc& desc) {
#if defined(AE_RHI_VULKAN_AVAILABLE) && AE_RHI_VULKAN_AVAILABLE
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

        for (u32 i = 0; i < colorCount; ++i) {
            const auto& color = desc.mColorAttachments[i];
            VkImageView view  = GetRenderTargetViewHandle(color.mView);
            if (!view || !color.mView || !color.mView->GetTexture()) {
                continue;
            }

            auto*          texture = color.mView->GetTexture();
            const VkFormat format  = Vulkan::Detail::ToVkFormat(texture->GetDesc().mFormat);
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
                auto* texture        = dsv->GetTexture();
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
#else
        (void)desc;
#endif
    }

    void FRhiVulkanCommandContext::RHIEndRenderPass() {
#if defined(AE_RHI_VULKAN_AVAILABLE) && AE_RHI_VULKAN_AVAILABLE
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
#endif
    }
