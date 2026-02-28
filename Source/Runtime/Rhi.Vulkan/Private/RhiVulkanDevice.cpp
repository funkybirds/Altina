#include "RhiVulkan/RhiVulkanDevice.h"

#include "RhiVulkanBackend.h"
#include "RhiVulkan/RhiVulkanCommandContext.h"
#include "RhiVulkan/RhiVulkanCommandList.h"
#include "RhiVulkan/RhiVulkanCommandPool.h"
#include "RhiVulkan/RhiVulkanPipeline.h"
#include "RhiVulkan/RhiVulkanResources.h"
#include "RhiVulkan/RhiVulkanStagingBufferManager.h"
#include "RhiVulkan/RhiVulkanUploadBufferManager.h"
#include "RhiVulkan/RhiVulkanViewport.h"

#include "Rhi/RhiInit.h"
#include "Types/CheckedCast.h"

#include "RhiVulkanInternal.h"
#include "RhiVulkanMemoryAllocator.h"
#include "Platform/Generic/GenericPlatformDecl.h"

using AltinaEngine::Move;

namespace AltinaEngine::Rhi {
#if defined(AE_RHI_VULKAN_AVAILABLE) && AE_RHI_VULKAN_AVAILABLE
    namespace {
        [[nodiscard]] auto SelectQueueFamilyIndex(VkPhysicalDevice physical, VkQueueFlags required,
            VkQueueFlags preferExclusive) noexcept -> u32 {
            u32 queueFamilyCount = 0;
            vkGetPhysicalDeviceQueueFamilyProperties(physical, &queueFamilyCount, nullptr);
            if (queueFamilyCount == 0) {
                return UINT32_MAX;
            }

            Core::Container::TVector<VkQueueFamilyProperties> families;
            families.Resize(queueFamilyCount);
            vkGetPhysicalDeviceQueueFamilyProperties(physical, &queueFamilyCount, families.Data());

            for (u32 i = 0; i < queueFamilyCount; ++i) {
                const auto& props = families[i];
                if ((props.queueFlags & required) != required) {
                    continue;
                }
                if ((props.queueFlags & preferExclusive) == required) {
                    return i;
                }
            }
            for (u32 i = 0; i < queueFamilyCount; ++i) {
                const auto& props = families[i];
                if ((props.queueFlags & required) == required) {
                    return i;
                }
            }
            return UINT32_MAX;
        }

        [[nodiscard]] auto ToQueueFlags(ERhiQueueType type) noexcept -> VkQueueFlags {
            switch (type) {
                case ERhiQueueType::Compute:
                    return VK_QUEUE_COMPUTE_BIT;
                case ERhiQueueType::Copy:
                    return VK_QUEUE_TRANSFER_BIT;
                case ERhiQueueType::Graphics:
                default:
                    return VK_QUEUE_GRAPHICS_BIT;
            }
        }

        [[nodiscard]] auto GetFormatBytesPerPixel(ERhiFormat fmt) noexcept -> u32 {
            switch (fmt) {
                case ERhiFormat::R8G8B8A8Unorm:
                case ERhiFormat::R8G8B8A8UnormSrgb:
                case ERhiFormat::B8G8R8A8Unorm:
                case ERhiFormat::B8G8R8A8UnormSrgb:
                case ERhiFormat::R32Float:
                    return 4U;
                case ERhiFormat::R16G16B16A16Float:
                    return 8U;
                case ERhiFormat::R32G32Float:
                    return 8U;
                case ERhiFormat::R32G32B32Float:
                    return 12U;
                case ERhiFormat::D24UnormS8Uint:
                case ERhiFormat::D32Float:
                default:
                    return 0U;
            }
        }

    } // namespace

    struct FRhiVulkanDevice::FState {
        VkInstance                             mInstance          = VK_NULL_HANDLE;
        VkPhysicalDevice                       mPhysicalDevice    = VK_NULL_HANDLE;
        VkDevice                               mDevice            = VK_NULL_HANDLE;
        VkQueue                                mGraphicsQueue     = VK_NULL_HANDLE;
        VkQueue                                mComputeQueue      = VK_NULL_HANDLE;
        VkQueue                                mTransferQueue     = VK_NULL_HANDLE;
        u32                                    mGraphicsFamily    = 0U;
        u32                                    mComputeFamily     = 0U;
        u32                                    mTransferFamily    = 0U;
        bool                                   mSupportsDynRender = false;
        bool                                   mSupportsSync2     = false;
        bool                                   mSupportsExtDyn    = false;

        FRhiVulkanCommandSubmitter             mSubmitter;
        Vulkan::Detail::FVulkanMemoryAllocator mAllocator;

        VkSemaphore                            mPendingAcquire        = VK_NULL_HANDLE;
        VkSemaphore                            mPendingRenderComplete = VK_NULL_HANDLE;

        FVulkanUploadBufferManager             mUploadManager;
        FVulkanStagingBufferManager            mStagingManager;
        u64                                    mOptimalCopyOffsetAlignment   = 16ULL;
        u64                                    mOptimalCopyRowPitchAlignment = 1ULL;
    };

    FRhiVulkanDevice::FRhiVulkanDevice(const FRhiDeviceDesc& desc,
        const FRhiAdapterDesc& adapterDesc, VkInstance instance, VkPhysicalDevice physicalDevice,
        VkDevice device, bool enableSync2, bool enableDynamicRendering,
        bool enableTimelineSemaphore, bool enableExtendedDynamicState)
        : FRhiDevice(desc, adapterDesc) {
        mState                  = new FState{};
        mState->mInstance       = instance;
        mState->mPhysicalDevice = physicalDevice;
        mState->mDevice         = device;

        mState->mGraphicsFamily =
            SelectQueueFamilyIndex(physicalDevice, VK_QUEUE_GRAPHICS_BIT, VK_QUEUE_GRAPHICS_BIT);
        mState->mComputeFamily =
            SelectQueueFamilyIndex(physicalDevice, VK_QUEUE_COMPUTE_BIT, VK_QUEUE_COMPUTE_BIT);
        mState->mTransferFamily =
            SelectQueueFamilyIndex(physicalDevice, VK_QUEUE_TRANSFER_BIT, VK_QUEUE_TRANSFER_BIT);

        if (mState->mGraphicsFamily == UINT32_MAX) {
            mState->mGraphicsFamily = 0U;
        }
        if (mState->mComputeFamily == UINT32_MAX) {
            mState->mComputeFamily = mState->mGraphicsFamily;
        }
        if (mState->mTransferFamily == UINT32_MAX) {
            mState->mTransferFamily = mState->mGraphicsFamily;
        }

        vkGetDeviceQueue(mState->mDevice, mState->mGraphicsFamily, 0, &mState->mGraphicsQueue);
        vkGetDeviceQueue(mState->mDevice, mState->mComputeFamily, 0, &mState->mComputeQueue);
        vkGetDeviceQueue(mState->mDevice, mState->mTransferFamily, 0, &mState->mTransferQueue);

        // Supported features/limits: keep conservative; extend as the backend grows.
        // Treat these as "enabled" rather than just "supported" to avoid using features that
        // weren't actually enabled at vkCreateDevice time.
        FRhiSupportedFeatures features{};
        features.mTimelineSemaphore = enableTimelineSemaphore;
        SetSupportedFeatures(features);

        mState->mSupportsSync2     = enableSync2;
        mState->mSupportsDynRender = enableDynamicRendering;
        mState->mSupportsExtDyn    = enableExtendedDynamicState;

        FRhiSupportedLimits        limits{};
        VkPhysicalDeviceProperties props{};
        vkGetPhysicalDeviceProperties(mState->mPhysicalDevice, &props);
        limits.mMaxBufferSize                  = props.limits.maxStorageBufferRange;
        limits.mMaxTextureDimension2D          = props.limits.maxImageDimension2D;
        limits.mMaxTextureDimension3D          = props.limits.maxImageDimension3D;
        limits.mMaxTextureArrayLayers          = props.limits.maxImageArrayLayers;
        limits.mMaxSamplers                    = props.limits.maxPerStageDescriptorSamplers;
        limits.mMaxBindGroups                  = props.limits.maxBoundDescriptorSets;
        limits.mMaxColorAttachments            = props.limits.maxColorAttachments;
        limits.mMaxComputeWorkgroupSizeX       = props.limits.maxComputeWorkGroupSize[0];
        limits.mMaxComputeWorkgroupSizeY       = props.limits.maxComputeWorkGroupSize[1];
        limits.mMaxComputeWorkgroupSizeZ       = props.limits.maxComputeWorkGroupSize[2];
        limits.mMaxComputeWorkgroupInvocations = props.limits.maxComputeWorkGroupInvocations;
        SetSupportedLimits(limits);

        mState->mOptimalCopyOffsetAlignment = (props.limits.optimalBufferCopyOffsetAlignment != 0)
            ? static_cast<u64>(props.limits.optimalBufferCopyOffsetAlignment)
            : 16ULL;
        mState->mOptimalCopyRowPitchAlignment =
            (props.limits.optimalBufferCopyRowPitchAlignment != 0)
            ? static_cast<u64>(props.limits.optimalBufferCopyRowPitchAlignment)
            : 1ULL;

        FRhiQueueCapabilities caps{};
        caps.mSupportsGraphics     = (mState->mGraphicsQueue != VK_NULL_HANDLE);
        caps.mSupportsCompute      = (mState->mComputeQueue != VK_NULL_HANDLE);
        caps.mSupportsCopy         = (mState->mTransferQueue != VK_NULL_HANDLE);
        caps.mSupportsAsyncCompute = (mState->mComputeFamily != mState->mGraphicsFamily);
        caps.mSupportsAsyncCopy    = (mState->mTransferFamily != mState->mGraphicsFamily);
        SetQueueCapabilities(caps);

        mState->mSubmitter.Start();

        RegisterQueue(ERhiQueueType::Graphics,
            FRhiQueueRef::Adopt(new FRhiVulkanQueue(
                ERhiQueueType::Graphics, mState->mGraphicsQueue, &mState->mSubmitter, this)));
        RegisterQueue(ERhiQueueType::Compute,
            FRhiQueueRef::Adopt(new FRhiVulkanQueue(
                ERhiQueueType::Compute, mState->mComputeQueue, &mState->mSubmitter, this)));
        RegisterQueue(ERhiQueueType::Copy,
            FRhiQueueRef::Adopt(new FRhiVulkanQueue(
                ERhiQueueType::Copy, mState->mTransferQueue, &mState->mSubmitter, this)));

        mState->mAllocator.Init(mState->mPhysicalDevice, mState->mDevice);

        FVulkanUploadBufferManagerDesc uploadDesc{};
        uploadDesc.mPageCount      = 3U;
        uploadDesc.mPageSizeBytes  = 4ULL * 1024ULL * 1024ULL;
        uploadDesc.mAlignmentBytes = mState->mOptimalCopyOffsetAlignment;
        mState->mUploadManager.Init(this, uploadDesc);
        mState->mStagingManager.Init(this);
    }

    FRhiVulkanDevice::~FRhiVulkanDevice() {
        if (mState) {
            FlushResourceDeleteQueue();

            mState->mUploadManager.EndFrame();
            mState->mUploadManager.Reset();
            mState->mStagingManager.Reset();

            mState->mSubmitter.Stop();

            mState->mAllocator.Shutdown();

            if (mState->mDevice) {
                vkDeviceWaitIdle(mState->mDevice);
                vkDestroyDevice(mState->mDevice, nullptr);
                mState->mDevice = VK_NULL_HANDLE;
            }
            delete mState;
            mState = nullptr;
        }
    }

    auto FRhiVulkanDevice::GetNativeInstance() const noexcept -> VkInstance {
        return mState ? mState->mInstance : VK_NULL_HANDLE;
    }

    auto FRhiVulkanDevice::GetPhysicalDevice() const noexcept -> VkPhysicalDevice {
        return mState ? mState->mPhysicalDevice : VK_NULL_HANDLE;
    }

    auto FRhiVulkanDevice::GetNativeDevice() const noexcept -> VkDevice {
        return mState ? mState->mDevice : VK_NULL_HANDLE;
    }

    auto FRhiVulkanDevice::GetQueueFamilyIndex(ERhiQueueType type) const noexcept -> u32 {
        if (!mState) {
            return 0U;
        }
        switch (type) {
            case ERhiQueueType::Compute:
                return mState->mComputeFamily;
            case ERhiQueueType::Copy:
                return mState->mTransferFamily;
            case ERhiQueueType::Graphics:
            default:
                return mState->mGraphicsFamily;
        }
    }

    auto FRhiVulkanDevice::SupportsDynamicRendering() const noexcept -> bool {
        return mState && mState->mSupportsDynRender;
    }

    auto FRhiVulkanDevice::SupportsSynchronization2() const noexcept -> bool {
        return mState && mState->mSupportsSync2;
    }

    auto FRhiVulkanDevice::SupportsExtendedDynamicState() const noexcept -> bool {
        return mState && mState->mSupportsExtDyn;
    }

    auto FRhiVulkanDevice::GetInternalAllocatorHandle() const noexcept -> void* {
        return mState ? static_cast<void*>(&mState->mAllocator) : nullptr;
    }

    void FRhiVulkanDevice::NotifyViewportAcquired(VkSemaphore acquire, VkSemaphore renderComplete) {
        if (!mState) {
            return;
        }
        mState->mPendingAcquire        = acquire;
        mState->mPendingRenderComplete = renderComplete;
    }

    auto FRhiVulkanDevice::ConsumePendingAcquireSemaphore() noexcept -> VkSemaphore {
        if (!mState) {
            return VK_NULL_HANDLE;
        }
        VkSemaphore out         = mState->mPendingAcquire;
        mState->mPendingAcquire = VK_NULL_HANDLE;
        return out;
    }

    auto FRhiVulkanDevice::ConsumePendingRenderCompleteSemaphore() noexcept -> VkSemaphore {
        if (!mState) {
            return VK_NULL_HANDLE;
        }
        VkSemaphore out                = mState->mPendingRenderComplete;
        mState->mPendingRenderComplete = VK_NULL_HANDLE;
        return out;
    }

    auto FRhiVulkanDevice::CreateBuffer(const FRhiBufferDesc& desc) -> FRhiBufferRef {
        return MakeResource<FRhiVulkanBuffer>(desc, mState ? mState->mDevice : VK_NULL_HANDLE);
    }

    auto FRhiVulkanDevice::CreateTexture(const FRhiTextureDesc& desc) -> FRhiTextureRef {
        return MakeResource<FRhiVulkanTexture>(desc, mState ? mState->mDevice : VK_NULL_HANDLE);
    }

    auto FRhiVulkanDevice::CreateShaderResourceView(const FRhiShaderResourceViewDesc& desc)
        -> FRhiShaderResourceViewRef {
        return MakeResource<FRhiVulkanShaderResourceView>(
            desc, mState ? mState->mDevice : VK_NULL_HANDLE);
    }

    auto FRhiVulkanDevice::CreateUnorderedAccessView(const FRhiUnorderedAccessViewDesc& desc)
        -> FRhiUnorderedAccessViewRef {
        return MakeResource<FRhiVulkanUnorderedAccessView>(
            desc, mState ? mState->mDevice : VK_NULL_HANDLE);
    }

    auto FRhiVulkanDevice::CreateRenderTargetView(const FRhiRenderTargetViewDesc& desc)
        -> FRhiRenderTargetViewRef {
        return MakeResource<FRhiVulkanRenderTargetView>(
            desc, mState ? mState->mDevice : VK_NULL_HANDLE);
    }

    auto FRhiVulkanDevice::CreateDepthStencilView(const FRhiDepthStencilViewDesc& desc)
        -> FRhiDepthStencilViewRef {
        return MakeResource<FRhiVulkanDepthStencilView>(
            desc, mState ? mState->mDevice : VK_NULL_HANDLE);
    }

    auto FRhiVulkanDevice::CreateViewport(const FRhiViewportDesc& desc) -> FRhiViewportRef {
        VkQueue queue  = mState ? mState->mGraphicsQueue : VK_NULL_HANDLE;
        u32     family = mState ? mState->mGraphicsFamily : 0U;
        return MakeResource<FRhiVulkanViewport>(
            desc, GetNativeInstance(), GetNativeDevice(), GetPhysicalDevice(), queue, family);
    }

    auto FRhiVulkanDevice::CreateSampler(const FRhiSamplerDesc& desc) -> FRhiSamplerRef {
        return MakeResource<FRhiVulkanSampler>(desc, mState ? mState->mDevice : VK_NULL_HANDLE);
    }

    auto FRhiVulkanDevice::CreateShader(const FRhiShaderDesc& desc) -> FRhiShaderRef {
        return MakeResource<FRhiVulkanShader>(desc, mState ? mState->mDevice : VK_NULL_HANDLE);
    }

    auto FRhiVulkanDevice::CreateGraphicsPipeline(const FRhiGraphicsPipelineDesc& desc)
        -> FRhiPipelineRef {
        const bool supportsExtDyn = mState && mState->mSupportsExtDyn;
        return MakeResource<FRhiVulkanGraphicsPipeline>(
            desc, mState ? mState->mDevice : VK_NULL_HANDLE, supportsExtDyn);
    }

    auto FRhiVulkanDevice::CreateComputePipeline(const FRhiComputePipelineDesc& desc)
        -> FRhiPipelineRef {
        return MakeResource<FRhiVulkanComputePipeline>(
            desc, mState ? mState->mDevice : VK_NULL_HANDLE);
    }

    auto FRhiVulkanDevice::CreatePipelineLayout(const FRhiPipelineLayoutDesc& desc)
        -> FRhiPipelineLayoutRef {
        return MakeResource<FRhiVulkanPipelineLayout>(
            desc, mState ? mState->mDevice : VK_NULL_HANDLE);
    }

    auto FRhiVulkanDevice::CreateBindGroupLayout(const FRhiBindGroupLayoutDesc& desc)
        -> FRhiBindGroupLayoutRef {
        return MakeResource<FRhiVulkanBindGroupLayout>(
            desc, mState ? mState->mDevice : VK_NULL_HANDLE);
    }

    auto FRhiVulkanDevice::CreateBindGroup(const FRhiBindGroupDesc& desc) -> FRhiBindGroupRef {
        // Descriptor set allocation is handled inside bind group; device is required for update
        // calls.
        return MakeResource<FRhiVulkanBindGroup>(
            desc, mState ? mState->mDevice : VK_NULL_HANDLE, VK_NULL_HANDLE);
    }

    void FRhiVulkanDevice::UpdateTextureSubresource(FRhiTexture* texture,
        const FRhiTextureSubresource& subresource, const void* data, u32 rowPitchBytes,
        u32 slicePitchBytes) {
        if (!mState || texture == nullptr || data == nullptr || rowPitchBytes == 0U) {
            return;
        }
        (void)slicePitchBytes;

        const FRhiTextureDesc& desc = texture->GetDesc();
        if (subresource.mMipLevel >= desc.mMipLevels) {
            return;
        }

        const auto mipDim = [](u32 dim, u32 mip) noexcept -> u32 {
            const u32 v = dim >> mip;
            return v ? v : 1U;
        };

        const u32 mipWidth  = mipDim(desc.mWidth, subresource.mMipLevel);
        const u32 mipHeight = mipDim(desc.mHeight, subresource.mMipLevel);
        const u32 mipDepth  = mipDim(desc.mDepth, subresource.mMipLevel);

        if (desc.mDimension == ERhiTextureDimension::Tex3D) {
            if (subresource.mArrayLayer != 0U || subresource.mDepthSlice >= mipDepth) {
                return;
            }
        } else {
            if (subresource.mDepthSlice != 0U || subresource.mArrayLayer >= desc.mArrayLayers) {
                return;
            }
        }

        const u32 bpp = GetFormatBytesPerPixel(desc.mFormat);
        if (bpp == 0U) {
            return;
        }
        if ((rowPitchBytes % bpp) != 0U) {
            return;
        }
        if ((rowPitchBytes / bpp) < mipWidth) {
            return;
        }

        const u64 uploadSizeBytes = static_cast<u64>(rowPitchBytes) * static_cast<u64>(mipHeight);
        if (uploadSizeBytes == 0ULL) {
            return;
        }

        // Respect the device alignment constraints for buffer-image copies.
        const u64               alignment = (mState->mOptimalCopyOffsetAlignment > 0ULL)
                          ? mState->mOptimalCopyOffsetAlignment
                          : 16ULL;

        FVulkanUploadAllocation uploadAlloc =
            mState->mUploadManager.Allocate(uploadSizeBytes, alignment, 0ULL);

        FRhiBuffer*              srcBuffer = nullptr;
        u64                      srcOffset = 0ULL;
        FVulkanStagingAllocation stagingAlloc{};

        if (uploadAlloc.IsValid()
            && mState->mUploadManager.Write(uploadAlloc, data, uploadSizeBytes, 0ULL)) {
            srcBuffer = uploadAlloc.mBuffer;
            srcOffset = uploadAlloc.mOffset;
        } else {
            stagingAlloc = mState->mStagingManager.Acquire(uploadSizeBytes, ERhiCpuAccess::Write);
            if (!stagingAlloc.IsValid()) {
                return;
            }
            void* mapped = mState->mStagingManager.Map(stagingAlloc, EVulkanStagingMapMode::Write);
            if (mapped == nullptr) {
                mState->mStagingManager.Release(stagingAlloc);
                return;
            }
            Core::Platform::Generic::Memcpy(mapped, data, static_cast<usize>(uploadSizeBytes));
            mState->mStagingManager.Unmap(stagingAlloc);
            srcBuffer = stagingAlloc.mBuffer;
            srcOffset = 0ULL;
        }

        auto* vkTex = static_cast<FRhiVulkanTexture*>(texture);
        auto* vkBuf = static_cast<FRhiVulkanBuffer*>(srcBuffer);
        if (!vkTex || !vkBuf) {
            if (stagingAlloc.IsValid()) {
                mState->mStagingManager.Release(stagingAlloc);
            }
            return;
        }

        VkCommandPool           pool = VK_NULL_HANDLE;
        VkCommandBuffer         cmd  = VK_NULL_HANDLE;

        VkCommandPoolCreateInfo poolInfo{};
        poolInfo.sType            = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        poolInfo.flags            = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
        poolInfo.queueFamilyIndex = mState->mGraphicsFamily;
        if (vkCreateCommandPool(mState->mDevice, &poolInfo, nullptr, &pool) != VK_SUCCESS) {
            if (stagingAlloc.IsValid()) {
                mState->mStagingManager.Release(stagingAlloc);
            }
            return;
        }

        VkCommandBufferAllocateInfo allocInfo{};
        allocInfo.sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocInfo.commandPool        = pool;
        allocInfo.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocInfo.commandBufferCount = 1;
        if (vkAllocateCommandBuffers(mState->mDevice, &allocInfo, &cmd) != VK_SUCCESS) {
            vkDestroyCommandPool(mState->mDevice, pool, nullptr);
            if (stagingAlloc.IsValid()) {
                mState->mStagingManager.Release(stagingAlloc);
            }
            return;
        }

        VkCommandBufferBeginInfo begin{};
        begin.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        begin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        vkBeginCommandBuffer(cmd, &begin);

        const bool              isDepth = Vulkan::Detail::IsDepthFormat(desc.mFormat);
        VkImageSubresourceRange range{};
        range.aspectMask   = Vulkan::Detail::ToVkAspectFlags(desc.mFormat);
        range.baseMipLevel = subresource.mMipLevel;
        range.levelCount   = 1;
        range.baseArrayLayer =
            (desc.mDimension == ERhiTextureDimension::Tex3D) ? 0U : subresource.mArrayLayer;
        range.layerCount = 1;

        VkImageMemoryBarrier toDst{};
        toDst.sType               = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        toDst.image               = vkTex->GetNativeImage();
        toDst.subresourceRange    = range;
        toDst.oldLayout           = VK_IMAGE_LAYOUT_UNDEFINED;
        toDst.newLayout           = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        toDst.srcAccessMask       = 0;
        toDst.dstAccessMask       = VK_ACCESS_TRANSFER_WRITE_BIT;
        toDst.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        toDst.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;

        vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
            0, 0, nullptr, 0, nullptr, 1, &toDst);

        VkBufferImageCopy copy{};
        copy.bufferOffset = srcOffset;

        // Vulkan expresses row pitch in texels. If the input is tightly packed, keep these at 0.
        const u32 rowLengthTexels            = static_cast<u32>(rowPitchBytes / bpp);
        copy.bufferRowLength                 = (rowLengthTexels == mipWidth) ? 0U : rowLengthTexels;
        copy.bufferImageHeight               = 0U;
        copy.imageSubresource.aspectMask     = range.aspectMask;
        copy.imageSubresource.mipLevel       = subresource.mMipLevel;
        copy.imageSubresource.baseArrayLayer = range.baseArrayLayer;
        copy.imageSubresource.layerCount     = 1;
        copy.imageOffset                     = { 0, 0,
                                static_cast<i32>(
                (desc.mDimension == ERhiTextureDimension::Tex3D) ? subresource.mDepthSlice : 0U) };
        copy.imageExtent                     = { mipWidth, mipHeight, 1U };

        vkCmdCopyBufferToImage(cmd, vkBuf->GetNativeBuffer(), vkTex->GetNativeImage(),
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copy);

        const auto common = Vulkan::Detail::MapResourceState(ERhiResourceState::Common, isDepth);
        VkImageMemoryBarrier toCommon{};
        toCommon.sType               = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        toCommon.image               = vkTex->GetNativeImage();
        toCommon.subresourceRange    = range;
        toCommon.oldLayout           = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        toCommon.newLayout           = common.mLayout;
        toCommon.srcAccessMask       = VK_ACCESS_TRANSFER_WRITE_BIT;
        toCommon.dstAccessMask       = 0;
        toCommon.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        toCommon.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;

        vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT,
            VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0, 0, nullptr, 0, nullptr, 1, &toCommon);

        vkEndCommandBuffer(cmd);

        VkFence           fence = VK_NULL_HANDLE;
        VkFenceCreateInfo fenceInfo{};
        fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        vkCreateFence(mState->mDevice, &fenceInfo, nullptr, &fence);

        VkSubmitInfo submit{};
        submit.sType              = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submit.commandBufferCount = 1;
        submit.pCommandBuffers    = &cmd;
        vkQueueSubmit(mState->mGraphicsQueue, 1, &submit, fence);

        if (fence) {
            vkWaitForFences(mState->mDevice, 1, &fence, VK_TRUE, UINT64_MAX);
            vkDestroyFence(mState->mDevice, fence, nullptr);
        } else {
            vkQueueWaitIdle(mState->mGraphicsQueue);
        }

        vkFreeCommandBuffers(mState->mDevice, pool, 1, &cmd);
        vkDestroyCommandPool(mState->mDevice, pool, nullptr);

        if (stagingAlloc.IsValid()) {
            mState->mStagingManager.Release(stagingAlloc);
        }
    }

    auto FRhiVulkanDevice::CreateFence(u64 initialValue) -> FRhiFenceRef {
        return MakeResource<FRhiVulkanFence>(initialValue);
    }

    auto FRhiVulkanDevice::CreateSemaphore(bool timeline, u64 initialValue) -> FRhiSemaphoreRef {
        return MakeResource<FRhiVulkanSemaphore>(
            mState ? mState->mDevice : VK_NULL_HANDLE, timeline, initialValue);
    }

    auto FRhiVulkanDevice::CreateCommandPool(const FRhiCommandPoolDesc& desc)
        -> FRhiCommandPoolRef {
        const u32  family    = GetQueueFamilyIndex(desc.mQueueType);
        const bool transient = true;
        return MakeResource<FRhiVulkanCommandPool>(
            desc, mState ? mState->mDevice : VK_NULL_HANDLE, family, transient);
    }

    auto FRhiVulkanDevice::CreateCommandList(const FRhiCommandListDesc& desc)
        -> FRhiCommandListRef {
        (void)mState;
        return MakeResource<FRhiVulkanCommandList>(desc);
    }

    auto FRhiVulkanDevice::CreateCommandContext(const FRhiCommandContextDesc& desc)
        -> FRhiCommandContextRef {
        FRhiCommandPoolDesc poolDesc{};
        poolDesc.mDebugName      = desc.mDebugName;
        poolDesc.mQueueType      = desc.mQueueType;
        auto                pool = CreateCommandPool(poolDesc);

        FRhiCommandListDesc listDesc{};
        listDesc.mDebugName = desc.mDebugName;
        listDesc.mQueueType = desc.mQueueType;
        listDesc.mListType  = desc.mListType;
        auto list           = CreateCommandList(listDesc);

        return MakeResource<FRhiVulkanCommandContext>(
            desc, mState ? mState->mDevice : VK_NULL_HANDLE, this, Move(pool), Move(list));
    }

    void FRhiVulkanDevice::BeginFrame(u64 frameIndex) {
        if (!mState) {
            return;
        }
        mState->mUploadManager.BeginFrame(frameIndex);
        mState->mStagingManager.Reset();
    }

    void FRhiVulkanDevice::EndFrame() {
        if (!mState) {
            return;
        }
        mState->mUploadManager.EndFrame();
    }

#else
    // Real implementation compiled out when Vulkan is unavailable. Stubs live in
    // RhiVulkanStubs.cpp.
#endif
} // namespace AltinaEngine::Rhi
