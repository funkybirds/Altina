#include "RhiVulkan/RhiVulkanResources.h"

#include "RhiVulkan/RhiVulkanDevice.h"
#include "RhiVulkanInternal.h"
#include "RhiVulkanMemoryAllocator.h"

#include "Rhi/RhiInit.h"

namespace AltinaEngine::Rhi {
    namespace {
        [[nodiscard]] auto GetAllocator() noexcept -> Vulkan::Detail::FVulkanMemoryAllocator* {
            auto* device = static_cast<FRhiVulkanDevice*>(RHIGetDevice());
            if (!device) {
                return nullptr;
            }
            return static_cast<Vulkan::Detail::FVulkanMemoryAllocator*>(
                device->GetInternalAllocatorHandle());
        }

        void FillConcurrentSharingMode(
            VkSharingMode& outMode, Core::Container::TVector<u32>& outFamilyIndices) noexcept {
            outMode = VK_SHARING_MODE_EXCLUSIVE;
            outFamilyIndices.Clear();

            auto* device = static_cast<FRhiVulkanDevice*>(RHIGetDevice());
            if (!device) {
                return;
            }

            const u32 graphics = device->GetQueueFamilyIndex(ERhiQueueType::Graphics);
            const u32 compute  = device->GetQueueFamilyIndex(ERhiQueueType::Compute);
            const u32 copy     = device->GetQueueFamilyIndex(ERhiQueueType::Copy);

            auto      addUnique = [&](u32 family) {
                for (const u32 v : outFamilyIndices) {
                    if (v == family) {
                        return;
                    }
                }
                outFamilyIndices.PushBack(family);
            };

            addUnique(graphics);
            addUnique(compute);
            addUnique(copy);

            if (outFamilyIndices.Size() > 1) {
                outMode = VK_SHARING_MODE_CONCURRENT;
            } else {
                outFamilyIndices.Clear();
            }
        }

        [[nodiscard]] auto ToVkViewType(ERhiTextureDimension dim, bool arrayed) noexcept
            -> VkImageViewType {
            switch (dim) {
                case ERhiTextureDimension::Tex2D:
                    return VK_IMAGE_VIEW_TYPE_2D;
                case ERhiTextureDimension::Tex2DArray:
                    return VK_IMAGE_VIEW_TYPE_2D_ARRAY;
                case ERhiTextureDimension::Tex3D:
                    return VK_IMAGE_VIEW_TYPE_3D;
                case ERhiTextureDimension::Cube:
                    return VK_IMAGE_VIEW_TYPE_CUBE;
                case ERhiTextureDimension::CubeArray:
                    return VK_IMAGE_VIEW_TYPE_CUBE_ARRAY;
                default:
                    return arrayed ? VK_IMAGE_VIEW_TYPE_2D_ARRAY : VK_IMAGE_VIEW_TYPE_2D;
            }
        }

        [[nodiscard]] auto ToVkImageType(ERhiTextureDimension dim) noexcept -> VkImageType {
            switch (dim) {
                case ERhiTextureDimension::Tex3D:
                    return VK_IMAGE_TYPE_3D;
                case ERhiTextureDimension::Tex2D:
                case ERhiTextureDimension::Tex2DArray:
                case ERhiTextureDimension::Cube:
                case ERhiTextureDimension::CubeArray:
                default:
                    return VK_IMAGE_TYPE_2D;
            }
        }

        [[nodiscard]] auto ToVkShaderStages(ERhiShaderStageFlags visibility) noexcept
            -> VkShaderStageFlags {
            VkShaderStageFlags out = 0;
            if (HasAnyFlags(visibility, ERhiShaderStageFlags::Vertex)) {
                out |= VK_SHADER_STAGE_VERTEX_BIT;
            }
            if (HasAnyFlags(visibility, ERhiShaderStageFlags::Pixel)) {
                out |= VK_SHADER_STAGE_FRAGMENT_BIT;
            }
            if (HasAnyFlags(visibility, ERhiShaderStageFlags::Compute)) {
                out |= VK_SHADER_STAGE_COMPUTE_BIT;
            }
            if (out == 0) {
                out = VK_SHADER_STAGE_ALL;
            }
            return out;
        }
    } // namespace

    struct FRhiVulkanBuffer::FState {
        VkDevice                                mDevice    = VK_NULL_HANDLE;
        VkBuffer                                mBuffer    = VK_NULL_HANDLE;
        Vulkan::Detail::FVulkanMemoryAllocator* mAllocator = nullptr;
        Vulkan::Detail::FVulkanMemoryAllocation mAlloc{};
        bool                                    mHostVisible = false;
    };

    FRhiVulkanBuffer::FRhiVulkanBuffer(const FRhiBufferDesc& desc, VkDevice device)
        : FRhiBuffer(desc) {
        mState             = new FState{};
        mState->mDevice    = device;
        mState->mAllocator = GetAllocator();

        if (!mState->mDevice || desc.mSizeBytes == 0) {
            return;
        }

        VkBufferCreateInfo info{};
        info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        info.size  = static_cast<VkDeviceSize>(desc.mSizeBytes);
        info.usage = Vulkan::Detail::ToVkBufferUsage(desc.mBindFlags);
        if (desc.mUsage == ERhiResourceUsage::Dynamic || desc.mUsage == ERhiResourceUsage::Staging
            || HasAnyFlags(desc.mCpuAccess, ERhiCpuAccess::Write)) {
            info.usage |= VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
            mState->mHostVisible = true;
        }

        Core::Container::TVector<u32> familyIndices;
        FillConcurrentSharingMode(info.sharingMode, familyIndices);
        if (!familyIndices.IsEmpty()) {
            info.queueFamilyIndexCount = static_cast<u32>(familyIndices.Size());
            info.pQueueFamilyIndices   = familyIndices.Data();
        }

        if (vkCreateBuffer(mState->mDevice, &info, nullptr, &mState->mBuffer) != VK_SUCCESS) {
            mState->mBuffer = VK_NULL_HANDLE;
            return;
        }

        VkMemoryRequirements req{};
        vkGetBufferMemoryRequirements(mState->mDevice, mState->mBuffer, &req);

        if (!mState->mAllocator) {
            return;
        }

        VkMemoryPropertyFlags flags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
        if (mState->mHostVisible) {
            flags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
        }

        mState->mAlloc = mState->mAllocator->Allocate(req, flags, desc.mDebugName);
        if (!mState->mAlloc.IsValid()) {
            return;
        }

        vkBindBufferMemory(
            mState->mDevice, mState->mBuffer, mState->mAlloc.mMemory, mState->mAlloc.mOffset);
    }

    FRhiVulkanBuffer::~FRhiVulkanBuffer() {
        if (!mState) {
            return;
        }
        if (mState->mAllocator) {
            mState->mAllocator->Free(mState->mAlloc);
        }
        if (mState->mDevice && mState->mBuffer) {
            vkDestroyBuffer(mState->mDevice, mState->mBuffer, nullptr);
        }
        delete mState;
        mState = nullptr;
    }

    auto FRhiVulkanBuffer::Lock(u64 offset, u64 size, ERhiBufferLockMode) -> FLockResult {
        FLockResult out{};
        if (!mState || !mState->mHostVisible || !mState->mAlloc.mMappedPtr || !mState->mBuffer) {
            return out;
        }
        if (offset + size > GetDesc().mSizeBytes) {
            return out;
        }
        out.mData   = static_cast<u8*>(mState->mAlloc.mMappedPtr) + offset;
        out.mSize   = size;
        out.mOffset = offset;
        return out;
    }

    void FRhiVulkanBuffer::Unlock(FLockResult&) {}

    auto FRhiVulkanBuffer::GetNativeBuffer() const noexcept -> VkBuffer {
        return (mState != nullptr) ? mState->mBuffer : VK_NULL_HANDLE;
    }

    struct FRhiVulkanTexture::FState {
        VkDevice                                mDevice        = VK_NULL_HANDLE;
        VkImage                                 mImage         = VK_NULL_HANDLE;
        VkImageView                             mDefaultView   = VK_NULL_HANDLE;
        VkImageLayout                           mCurrentLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        bool                                    mOwnsImage     = true;
        Vulkan::Detail::FVulkanMemoryAllocator* mAllocator     = nullptr;
        Vulkan::Detail::FVulkanMemoryAllocation mAlloc{};
        FRhiSemaphore*                          mPendingUploadSemaphore = nullptr;
        u64                                     mPendingUploadValue     = 0ULL;
        bool                                    mHasPendingUpload       = false;
    };

    FRhiVulkanTexture::FRhiVulkanTexture(const FRhiTextureDesc& desc, VkDevice device)
        : FRhiTexture(desc) {
        mState             = new FState{};
        mState->mDevice    = device;
        mState->mAllocator = GetAllocator();

        if (!mState->mDevice || desc.mWidth == 0 || desc.mHeight == 0) {
            return;
        }

        const VkFormat    vkFormat = Vulkan::Detail::ToVkFormat(desc.mFormat);

        VkImageCreateInfo info{};
        info.sType         = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        info.imageType     = ToVkImageType(desc.mDimension);
        info.format        = vkFormat;
        info.extent.width  = desc.mWidth;
        info.extent.height = desc.mHeight;
        info.extent.depth  = (desc.mDimension == ERhiTextureDimension::Tex3D) ? desc.mDepth : 1U;
        info.mipLevels     = desc.mMipLevels;
        info.arrayLayers   = desc.mArrayLayers;
        info.samples       = VK_SAMPLE_COUNT_1_BIT;
        info.tiling        = VK_IMAGE_TILING_OPTIMAL;
        info.usage         = Vulkan::Detail::ToVkImageUsage(desc.mBindFlags)
            | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
        info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        if (desc.mDimension == ERhiTextureDimension::Cube
            || desc.mDimension == ERhiTextureDimension::CubeArray) {
            info.flags |= VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
        }

        Core::Container::TVector<u32> familyIndices;
        FillConcurrentSharingMode(info.sharingMode, familyIndices);
        if (!familyIndices.IsEmpty()) {
            info.queueFamilyIndexCount = static_cast<u32>(familyIndices.Size());
            info.pQueueFamilyIndices   = familyIndices.Data();
        }

        if (vkCreateImage(mState->mDevice, &info, nullptr, &mState->mImage) != VK_SUCCESS) {
            mState->mImage = VK_NULL_HANDLE;
            return;
        }

        VkMemoryRequirements req{};
        vkGetImageMemoryRequirements(mState->mDevice, mState->mImage, &req);

        VkMemoryPropertyFlags flags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
        if (HasAnyFlags(desc.mCpuAccess, ERhiCpuAccess::Write)
            || desc.mUsage == ERhiResourceUsage::Dynamic
            || desc.mUsage == ERhiResourceUsage::Staging) {
            flags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
        }

        if (!mState->mAllocator) {
            if (mState->mImage != VK_NULL_HANDLE) {
                vkDestroyImage(mState->mDevice, mState->mImage, nullptr);
                mState->mImage = VK_NULL_HANDLE;
            }
            return;
        }

        mState->mAlloc = mState->mAllocator->Allocate(req, flags, desc.mDebugName);
        if (!mState->mAlloc.IsValid()) {
            if (mState->mImage != VK_NULL_HANDLE) {
                vkDestroyImage(mState->mDevice, mState->mImage, nullptr);
                mState->mImage = VK_NULL_HANDLE;
            }
            return;
        }

        if (vkBindImageMemory(
                mState->mDevice, mState->mImage, mState->mAlloc.mMemory, mState->mAlloc.mOffset)
            != VK_SUCCESS) {
            if (mState->mAllocator && mState->mAlloc.IsValid()) {
                mState->mAllocator->Free(mState->mAlloc);
                mState->mAlloc = {};
            }
            if (mState->mImage != VK_NULL_HANDLE) {
                vkDestroyImage(mState->mDevice, mState->mImage, nullptr);
                mState->mImage = VK_NULL_HANDLE;
            }
            return;
        }

        VkImageViewCreateInfo view{};
        view.sType                           = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        view.image                           = mState->mImage;
        view.viewType                        = ToVkViewType(desc.mDimension, desc.mArrayLayers > 1);
        view.format                          = vkFormat;
        view.subresourceRange.aspectMask     = Vulkan::Detail::ToVkAspectFlags(desc.mFormat);
        view.subresourceRange.baseMipLevel   = 0;
        view.subresourceRange.levelCount     = desc.mMipLevels;
        view.subresourceRange.baseArrayLayer = 0;
        view.subresourceRange.layerCount     = desc.mArrayLayers;
        if (vkCreateImageView(mState->mDevice, &view, nullptr, &mState->mDefaultView)
            != VK_SUCCESS) {
            mState->mDefaultView = VK_NULL_HANDLE;
            if (mState->mAllocator && mState->mAlloc.IsValid()) {
                mState->mAllocator->Free(mState->mAlloc);
                mState->mAlloc = {};
            }
            if (mState->mImage != VK_NULL_HANDLE) {
                vkDestroyImage(mState->mDevice, mState->mImage, nullptr);
                mState->mImage = VK_NULL_HANDLE;
            }
        }
    }

    FRhiVulkanTexture::FRhiVulkanTexture(const FRhiTextureDesc& desc, VkDevice device,
        VkImage image, VkImageView view, bool ownsImage)
        : FRhiTexture(desc) {
        mState               = new FState{};
        mState->mDevice      = device;
        mState->mImage       = image;
        mState->mDefaultView = view;
        mState->mOwnsImage   = ownsImage;
        mState->mAllocator   = GetAllocator();
    }

    FRhiVulkanTexture::~FRhiVulkanTexture() {
        if (!mState) {
            return;
        }
        if (mState->mDevice && mState->mDefaultView) {
            vkDestroyImageView(mState->mDevice, mState->mDefaultView, nullptr);
        }
        if (mState->mAllocator && mState->mAlloc.IsValid()) {
            mState->mAllocator->Free(mState->mAlloc);
        }
        if (mState->mDevice && mState->mImage && mState->mOwnsImage) {
            vkDestroyImage(mState->mDevice, mState->mImage, nullptr);
        }
        delete mState;
        mState = nullptr;
    }

    auto FRhiVulkanTexture::GetNativeImage() const noexcept -> VkImage {
        return (mState != nullptr) ? mState->mImage : VK_NULL_HANDLE;
    }

    auto FRhiVulkanTexture::GetDefaultView() const noexcept -> VkImageView {
        return (mState != nullptr) ? mState->mDefaultView : VK_NULL_HANDLE;
    }

    auto FRhiVulkanTexture::GetCurrentLayout() const noexcept -> VkImageLayout {
        return (mState != nullptr) ? mState->mCurrentLayout : VK_IMAGE_LAYOUT_UNDEFINED;
    }

    void FRhiVulkanTexture::SetCurrentLayout(VkImageLayout layout) noexcept {
        if (mState == nullptr) {
            return;
        }
        mState->mCurrentLayout = layout;
    }

    void FRhiVulkanTexture::SetPendingUpload(FRhiSemaphore* semaphore, u64 value) noexcept {
        if (!mState) {
            return;
        }
        mState->mPendingUploadSemaphore = semaphore;
        mState->mPendingUploadValue     = value;
        mState->mHasPendingUpload       = (semaphore != nullptr);
    }

    auto FRhiVulkanTexture::HasPendingUpload() const noexcept -> bool {
        return mState != nullptr && mState->mHasPendingUpload;
    }

    auto FRhiVulkanTexture::GetPendingUpload(
        FRhiSemaphore*& outSemaphore, u64& outValue) const noexcept -> bool {
        if (!mState || !mState->mHasPendingUpload || mState->mPendingUploadSemaphore == nullptr) {
            outSemaphore = nullptr;
            outValue     = 0ULL;
            return false;
        }
        outSemaphore = mState->mPendingUploadSemaphore;
        outValue     = mState->mPendingUploadValue;
        return true;
    }

    void FRhiVulkanTexture::ClearPendingUpload() noexcept {
        if (!mState) {
            return;
        }
        mState->mPendingUploadSemaphore = nullptr;
        mState->mPendingUploadValue     = 0ULL;
        mState->mHasPendingUpload       = false;
    }

    struct FRhiVulkanSampler::FState {
        VkDevice  mDevice  = VK_NULL_HANDLE;
        VkSampler mSampler = VK_NULL_HANDLE;
    };

    FRhiVulkanSampler::FRhiVulkanSampler(const FRhiSamplerDesc& desc, VkDevice device)
        : FRhiSampler(desc) {
        mState          = new FState{};
        mState->mDevice = device;
        if (!mState->mDevice) {
            return;
        }

        VkSamplerCreateInfo info{};
        info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        info.magFilter =
            (desc.mFilter == ERhiSamplerFilter::Nearest) ? VK_FILTER_NEAREST : VK_FILTER_LINEAR;
        info.minFilter  = info.magFilter;
        info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
        auto toAddress  = [](ERhiSamplerAddressMode mode) -> VkSamplerAddressMode {
            return (mode == ERhiSamplerAddressMode::Clamp) ? VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE
                                                            : VK_SAMPLER_ADDRESS_MODE_REPEAT;
        };
        info.addressModeU     = toAddress(desc.mAddressU);
        info.addressModeV     = toAddress(desc.mAddressV);
        info.addressModeW     = toAddress(desc.mAddressW);
        info.maxAnisotropy    = 1.0f;
        info.anisotropyEnable = VK_FALSE;
        info.maxLod           = VK_LOD_CLAMP_NONE;

        if (vkCreateSampler(mState->mDevice, &info, nullptr, &mState->mSampler) != VK_SUCCESS) {
            mState->mSampler = VK_NULL_HANDLE;
        }
    }

    FRhiVulkanSampler::~FRhiVulkanSampler() {
        if (!mState) {
            return;
        }
        if (mState->mDevice && mState->mSampler) {
            vkDestroySampler(mState->mDevice, mState->mSampler, nullptr);
        }
        delete mState;
        mState = nullptr;
    }

    auto FRhiVulkanSampler::GetNativeSampler() const noexcept -> VkSampler {
        return (mState != nullptr) ? mState->mSampler : VK_NULL_HANDLE;
    }

    struct FRhiVulkanShader::FState {
        VkDevice       mDevice = VK_NULL_HANDLE;
        VkShaderModule mModule = VK_NULL_HANDLE;
    };

    FRhiVulkanShader::FRhiVulkanShader(const FRhiShaderDesc& desc, VkDevice device)
        : FRhiShader(desc) {
        mState          = new FState{};
        mState->mDevice = device;
        if (!mState->mDevice || desc.mBytecode.IsEmpty()) {
            return;
        }

        VkShaderModuleCreateInfo info{};
        info.sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        info.codeSize = static_cast<size_t>(desc.mBytecode.Size());
        info.pCode    = reinterpret_cast<const u32*>(desc.mBytecode.Data());
        if (vkCreateShaderModule(mState->mDevice, &info, nullptr, &mState->mModule) != VK_SUCCESS) {
            mState->mModule = VK_NULL_HANDLE;
        }
    }

    FRhiVulkanShader::~FRhiVulkanShader() {
        if (!mState) {
            return;
        }
        if (mState->mDevice && mState->mModule) {
            vkDestroyShaderModule(mState->mDevice, mState->mModule, nullptr);
        }
        delete mState;
        mState = nullptr;
    }

    auto FRhiVulkanShader::GetModule() const noexcept -> VkShaderModule {
        return (mState != nullptr) ? mState->mModule : VK_NULL_HANDLE;
    }

    struct FRhiVulkanShaderResourceView::FState {
        VkDevice    mDevice = VK_NULL_HANDLE;
        VkImageView mView   = VK_NULL_HANDLE;
    };

    FRhiVulkanShaderResourceView::FRhiVulkanShaderResourceView(
        const FRhiShaderResourceViewDesc& desc, VkDevice device)
        : FRhiShaderResourceView(desc) {
        mState          = new FState{};
        mState->mDevice = device;
        if (!mState->mDevice || desc.mTexture == nullptr) {
            return;
        }
        auto* vkTex = static_cast<FRhiVulkanTexture*>(desc.mTexture);
        if (!vkTex || vkTex->GetNativeImage() == VK_NULL_HANDLE) {
            return;
        }

        const auto&           texDesc = desc.mTexture->GetDesc();

        VkImageViewCreateInfo view{};
        view.sType    = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        view.image    = vkTex->GetNativeImage();
        view.viewType = ToVkViewType(texDesc.mDimension, texDesc.mArrayLayers > 1);
        view.format   = Vulkan::Detail::ToVkFormat(texDesc.mFormat);
        view.subresourceRange.aspectMask     = Vulkan::Detail::ToVkAspectFlags(texDesc.mFormat);
        view.subresourceRange.baseMipLevel   = 0;
        view.subresourceRange.levelCount     = texDesc.mMipLevels;
        view.subresourceRange.baseArrayLayer = 0;
        view.subresourceRange.layerCount     = texDesc.mArrayLayers;

        if (vkCreateImageView(mState->mDevice, &view, nullptr, &mState->mView) != VK_SUCCESS) {
            mState->mView = VK_NULL_HANDLE;
        }
    }

    FRhiVulkanShaderResourceView::~FRhiVulkanShaderResourceView() {
        if (!mState) {
            return;
        }
        if (mState->mDevice && mState->mView) {
            vkDestroyImageView(mState->mDevice, mState->mView, nullptr);
        }
        delete mState;
        mState = nullptr;
    }

    auto FRhiVulkanShaderResourceView::GetImageView() const noexcept -> VkImageView {
        return (mState != nullptr) ? mState->mView : VK_NULL_HANDLE;
    }

    struct FRhiVulkanUnorderedAccessView::FState {
        VkDevice    mDevice = VK_NULL_HANDLE;
        VkImageView mView   = VK_NULL_HANDLE;
    };

    FRhiVulkanUnorderedAccessView::FRhiVulkanUnorderedAccessView(
        const FRhiUnorderedAccessViewDesc& desc, VkDevice device)
        : FRhiUnorderedAccessView(desc) {
        mState          = new FState{};
        mState->mDevice = device;
        if (!mState->mDevice || desc.mTexture == nullptr) {
            return;
        }
        auto* vkTex = static_cast<FRhiVulkanTexture*>(desc.mTexture);
        if (!vkTex || vkTex->GetNativeImage() == VK_NULL_HANDLE) {
            return;
        }

        const auto&           texDesc = desc.mTexture->GetDesc();

        VkImageViewCreateInfo view{};
        view.sType    = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        view.image    = vkTex->GetNativeImage();
        view.viewType = ToVkViewType(texDesc.mDimension, texDesc.mArrayLayers > 1);
        view.format   = Vulkan::Detail::ToVkFormat(texDesc.mFormat);
        view.subresourceRange.aspectMask     = Vulkan::Detail::ToVkAspectFlags(texDesc.mFormat);
        view.subresourceRange.baseMipLevel   = 0;
        view.subresourceRange.levelCount     = texDesc.mMipLevels;
        view.subresourceRange.baseArrayLayer = 0;
        view.subresourceRange.layerCount     = texDesc.mArrayLayers;

        if (vkCreateImageView(mState->mDevice, &view, nullptr, &mState->mView) != VK_SUCCESS) {
            mState->mView = VK_NULL_HANDLE;
        }
    }

    FRhiVulkanUnorderedAccessView::~FRhiVulkanUnorderedAccessView() {
        if (!mState) {
            return;
        }
        if (mState->mDevice && mState->mView) {
            vkDestroyImageView(mState->mDevice, mState->mView, nullptr);
        }
        delete mState;
        mState = nullptr;
    }

    auto FRhiVulkanUnorderedAccessView::GetImageView() const noexcept -> VkImageView {
        return (mState != nullptr) ? mState->mView : VK_NULL_HANDLE;
    }

    struct FRhiVulkanRenderTargetView::FState {
        VkDevice    mDevice = VK_NULL_HANDLE;
        VkImageView mView   = VK_NULL_HANDLE;
    };

    FRhiVulkanRenderTargetView::FRhiVulkanRenderTargetView(
        const FRhiRenderTargetViewDesc& desc, VkDevice device)
        : FRhiRenderTargetView(desc) {
        mState          = new FState{};
        mState->mDevice = device;
        if (!mState->mDevice || desc.mTexture == nullptr) {
            return;
        }
        auto* vkTex = static_cast<FRhiVulkanTexture*>(desc.mTexture);
        if (!vkTex || vkTex->GetNativeImage() == VK_NULL_HANDLE) {
            return;
        }

        const auto&           texDesc = desc.mTexture->GetDesc();

        VkImageViewCreateInfo view{};
        view.sType    = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        view.image    = vkTex->GetNativeImage();
        view.viewType = ToVkViewType(texDesc.mDimension, texDesc.mArrayLayers > 1);
        view.format   = Vulkan::Detail::ToVkFormat(texDesc.mFormat);
        view.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
        view.subresourceRange.baseMipLevel   = 0;
        view.subresourceRange.levelCount     = texDesc.mMipLevels;
        view.subresourceRange.baseArrayLayer = 0;
        view.subresourceRange.layerCount     = texDesc.mArrayLayers;

        if (vkCreateImageView(mState->mDevice, &view, nullptr, &mState->mView) != VK_SUCCESS) {
            mState->mView = VK_NULL_HANDLE;
        }
    }

    FRhiVulkanRenderTargetView::~FRhiVulkanRenderTargetView() {
        if (!mState) {
            return;
        }
        if (mState->mDevice && mState->mView) {
            vkDestroyImageView(mState->mDevice, mState->mView, nullptr);
        }
        delete mState;
        mState = nullptr;
    }

    auto FRhiVulkanRenderTargetView::GetImageView() const noexcept -> VkImageView {
        return (mState != nullptr) ? mState->mView : VK_NULL_HANDLE;
    }

    struct FRhiVulkanDepthStencilView::FState {
        VkDevice    mDevice = VK_NULL_HANDLE;
        VkImageView mView   = VK_NULL_HANDLE;
    };

    FRhiVulkanDepthStencilView::FRhiVulkanDepthStencilView(
        const FRhiDepthStencilViewDesc& desc, VkDevice device)
        : FRhiDepthStencilView(desc) {
        mState          = new FState{};
        mState->mDevice = device;
        if (!mState->mDevice || desc.mTexture == nullptr) {
            return;
        }
        auto* vkTex = static_cast<FRhiVulkanTexture*>(desc.mTexture);
        if (!vkTex || vkTex->GetNativeImage() == VK_NULL_HANDLE) {
            return;
        }

        const auto&           texDesc = desc.mTexture->GetDesc();

        VkImageViewCreateInfo view{};
        view.sType    = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        view.image    = vkTex->GetNativeImage();
        view.viewType = ToVkViewType(texDesc.mDimension, texDesc.mArrayLayers > 1);
        view.format   = Vulkan::Detail::ToVkFormat(texDesc.mFormat);
        view.subresourceRange.aspectMask     = Vulkan::Detail::ToVkAspectFlags(texDesc.mFormat);
        view.subresourceRange.baseMipLevel   = 0;
        view.subresourceRange.levelCount     = texDesc.mMipLevels;
        view.subresourceRange.baseArrayLayer = 0;
        view.subresourceRange.layerCount     = texDesc.mArrayLayers;

        if (vkCreateImageView(mState->mDevice, &view, nullptr, &mState->mView) != VK_SUCCESS) {
            mState->mView = VK_NULL_HANDLE;
        }
    }

    FRhiVulkanDepthStencilView::~FRhiVulkanDepthStencilView() {
        if (!mState) {
            return;
        }
        if (mState->mDevice && mState->mView) {
            vkDestroyImageView(mState->mDevice, mState->mView, nullptr);
        }
        delete mState;
        mState = nullptr;
    }

    auto FRhiVulkanDepthStencilView::GetImageView() const noexcept -> VkImageView {
        return (mState != nullptr) ? mState->mView : VK_NULL_HANDLE;
    }

} // namespace AltinaEngine::Rhi
