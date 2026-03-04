#include "RhiVulkan/RhiVulkanViewport.h"

#include "RhiVulkan/RhiVulkanDevice.h"
#include "RhiVulkan/RhiVulkanResources.h"
#include "RhiVulkanInternal.h"

#include "Rhi/RhiInit.h"
#include "Rhi/RhiQueue.h"
#include "Logging/Log.h"
#include "Math/Common.h"
#include "Threading/Mutex.h"

#if AE_PLATFORM_WIN
    #ifndef WIN32_LEAN_AND_MEAN
        #define WIN32_LEAN_AND_MEAN
    #endif
    #ifndef NOMINMAX
        #define NOMINMAX
    #endif
    #include <Windows.h>
    #ifdef CreateSemaphore
        #undef CreateSemaphore
    #endif
    #include <vulkan/vulkan_win32.h>
#endif

using AltinaEngine::Move;
namespace AltinaEngine::Rhi {
    namespace Container = Core::Container;
    using Container::TVector;

    namespace {
#if AE_PLATFORM_WIN
        [[nodiscard]] auto GetQueueApiMutexHandle() noexcept -> HANDLE {
            static HANDLE sMutex =
                ::CreateMutexW(nullptr, FALSE, L"AltinaEngine.Rhi.Vulkan.QueueApiMutex");
            return sMutex;
        }
#else
        [[nodiscard]] auto GetQueueApiMutex() noexcept -> Core::Threading::FMutex& {
            static Core::Threading::FMutex sMutex;
            return sMutex;
        }
#endif

        class FQueueApiScopedLock final {
        public:
            FQueueApiScopedLock() noexcept { Lock(); }
            ~FQueueApiScopedLock() noexcept { Unlock(); }

            FQueueApiScopedLock(const FQueueApiScopedLock&)                    = delete;
            auto operator=(const FQueueApiScopedLock&) -> FQueueApiScopedLock& = delete;

        private:
            void Lock() noexcept {
#if AE_PLATFORM_WIN
                HANDLE sMutex = GetQueueApiMutexHandle();
                if (sMutex != nullptr) {
                    (void)::WaitForSingleObject(sMutex, INFINITE);
                }
#else
                GetQueueApiMutex().Lock();
#endif
            }

            void Unlock() noexcept {
#if AE_PLATFORM_WIN
                HANDLE sMutex = GetQueueApiMutexHandle();
                if (sMutex != nullptr) {
                    (void)::ReleaseMutex(sMutex);
                }
#else
                GetQueueApiMutex().Unlock();
#endif
            }
        };

        // Use an infinite timeout to avoid transient 1-second acquire timeouts starving rendering.
        constexpr u64                kAcquireTimeoutNs = ~0ULL;

        [[nodiscard]] constexpr auto IsSwapchainOutOfDate(VkResult result) noexcept -> bool {
            return result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR;
        }

        [[nodiscard]] auto PickPresentMode(VkPhysicalDevice physical, VkSurfaceKHR surface,
            bool allowTearing) -> VkPresentModeKHR {
            u32 count = 0;
            vkGetPhysicalDeviceSurfacePresentModesKHR(physical, surface, &count, nullptr);
            TVector<VkPresentModeKHR> modes;
            modes.Resize(count);
            if (count > 0) {
                vkGetPhysicalDeviceSurfacePresentModesKHR(physical, surface, &count, modes.Data());
            }

            if (allowTearing) {
                for (auto m : modes) {
                    if (m == VK_PRESENT_MODE_IMMEDIATE_KHR) {
                        return m;
                    }
                }
                for (auto m : modes) {
                    if (m == VK_PRESENT_MODE_MAILBOX_KHR) {
                        return m;
                    }
                }
            }

            // Required by the spec.
            return VK_PRESENT_MODE_FIFO_KHR;
        }

        [[nodiscard]] auto CreateBinarySemaphore(VkDevice device) -> VkSemaphore {
            VkSemaphoreCreateInfo info{};
            info.sType      = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
            VkSemaphore sem = VK_NULL_HANDLE;
            if (vkCreateSemaphore(device, &info, nullptr, &sem) != VK_SUCCESS) {
                return VK_NULL_HANDLE;
            }
            return sem;
        }

        void WaitViewportGpuIdle(FRhiVulkanDevice* owner, VkQueue graphicsQueue) {
            if (owner != nullptr) {
                auto queue = owner->GetQueue(ERhiQueueType::Graphics);
                if (queue) {
                    queue->WaitIdle();
                    return;
                }
            }
            if (graphicsQueue != VK_NULL_HANDLE) {
                FQueueApiScopedLock lock;
                vkQueueWaitIdle(graphicsQueue);
            }
        }
    } // namespace

    struct FRhiVulkanViewport::FState {
        VkInstance              mInstance       = VK_NULL_HANDLE;
        VkDevice                mDevice         = VK_NULL_HANDLE;
        VkPhysicalDevice        mPhysicalDevice = VK_NULL_HANDLE;
        VkQueue                 mGraphicsQueue  = VK_NULL_HANDLE;
        u32                     mGraphicsFamily = 0U;

        VkSurfaceKHR            mSurface   = VK_NULL_HANDLE;
        VkSwapchainKHR          mSwapchain = VK_NULL_HANDLE;
        VkFormat                mFormat    = VK_FORMAT_B8G8R8A8_UNORM;
        VkExtent2D              mExtent{};

        TVector<VkImage>        mImages;
        TVector<FRhiTextureRef> mBackBuffers;

        TVector<VkSemaphore>    mAcquireSemaphores;
        TVector<VkSemaphore>    mRenderCompleteSemaphores;
        TVector<VkFence>        mAcquireFences;
        u64                     mFrameIndex = 0ULL;

        u32                     mImageIndex = 0U;
        bool                    mAcquired   = false;
        FRhiVulkanDevice*       mOwner      = nullptr;
    };

    FRhiVulkanViewport::FRhiVulkanViewport(const FRhiViewportDesc& desc, VkInstance instance,
        VkDevice device, VkPhysicalDevice physicalDevice, VkQueue graphicsQueue,
        u32 graphicsQueueFamily)
        : FRhiViewport(desc) {
        mState                  = new FState{};
        mState->mInstance       = instance;
        mState->mDevice         = device;
        mState->mPhysicalDevice = physicalDevice;
        mState->mGraphicsQueue  = graphicsQueue;
        mState->mGraphicsFamily = graphicsQueueFamily;
        mState->mOwner          = static_cast<FRhiVulkanDevice*>(RHIGetDevice());

        if (!mState->mInstance || !mState->mDevice || !mState->mPhysicalDevice) {
            return;
        }

        auto createSwapchain = [&](const FRhiViewportDesc& viewportDesc) -> bool {
            FState& s = *mState;
            if (!s.mDevice || !s.mPhysicalDevice || !s.mSurface) {
                return false;
            }

            VkSurfaceCapabilitiesKHR caps{};
            if (vkGetPhysicalDeviceSurfaceCapabilitiesKHR(s.mPhysicalDevice, s.mSurface, &caps)
                != VK_SUCCESS) {
                return false;
            }

            u32 formatCount = 0;
            vkGetPhysicalDeviceSurfaceFormatsKHR(
                s.mPhysicalDevice, s.mSurface, &formatCount, nullptr);
            TVector<VkSurfaceFormatKHR> formats;
            formats.Resize(formatCount);
            if (formatCount > 0) {
                vkGetPhysicalDeviceSurfaceFormatsKHR(
                    s.mPhysicalDevice, s.mSurface, &formatCount, formats.Data());
            }

            VkSurfaceFormatKHR chosen{};
            chosen.format     = Vulkan::Detail::ToVkFormat(viewportDesc.mFormat);
            chosen.colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
            bool found        = false;
            for (const auto& f : formats) {
                if (f.format == chosen.format) {
                    chosen = f;
                    found  = true;
                    break;
                }
            }
            if (!found && !formats.IsEmpty()) {
                chosen = formats[0];
            }
            s.mFormat = chosen.format;

            VkPresentModeKHR presentMode =
                PickPresentMode(s.mPhysicalDevice, s.mSurface, viewportDesc.mAllowTearing);

            VkExtent2D extent{};
            extent.width  = viewportDesc.mWidth;
            extent.height = viewportDesc.mHeight;
            if (caps.currentExtent.width != UINT32_MAX) {
                extent = caps.currentExtent;
            } else {
                extent.width = Core::Math::Clamp(
                    extent.width, caps.minImageExtent.width, caps.maxImageExtent.width);
                extent.height = Core::Math::Clamp(
                    extent.height, caps.minImageExtent.height, caps.maxImageExtent.height);
            }
            s.mExtent = extent;

            u32 imageCount = viewportDesc.mBufferCount;
            if (imageCount < caps.minImageCount) {
                imageCount = caps.minImageCount;
            }
            if (caps.maxImageCount > 0 && imageCount > caps.maxImageCount) {
                imageCount = caps.maxImageCount;
            }

            VkSwapchainCreateInfoKHR info{};
            info.sType            = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
            info.surface          = s.mSurface;
            info.minImageCount    = imageCount;
            info.imageFormat      = chosen.format;
            info.imageColorSpace  = chosen.colorSpace;
            info.imageExtent      = extent;
            info.imageArrayLayers = 1;
            info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
            info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
            info.preTransform     = caps.currentTransform;
            info.compositeAlpha   = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
            info.presentMode      = presentMode;
            info.clipped          = VK_TRUE;
            info.oldSwapchain     = s.mSwapchain;

            VkSwapchainKHR newSwapchain = VK_NULL_HANDLE;
            if (vkCreateSwapchainKHR(s.mDevice, &info, nullptr, &newSwapchain) != VK_SUCCESS) {
                return false;
            }

            if (s.mSwapchain) {
                vkDestroySwapchainKHR(s.mDevice, s.mSwapchain, nullptr);
            }
            s.mSwapchain = newSwapchain;

            u32 outCount = 0;
            vkGetSwapchainImagesKHR(s.mDevice, s.mSwapchain, &outCount, nullptr);
            s.mImages.Resize(outCount);
            vkGetSwapchainImagesKHR(s.mDevice, s.mSwapchain, &outCount, s.mImages.Data());

            s.mBackBuffers.Clear();
            s.mBackBuffers.Reserve(outCount);
            for (u32 i = 0; i < outCount; ++i) {
                FRhiTextureDesc texDesc{};
                texDesc.mDebugName = viewportDesc.mDebugName;
                texDesc.mWidth     = extent.width;
                texDesc.mHeight    = extent.height;
                texDesc.mFormat    = viewportDesc.mFormat;
                texDesc.mBindFlags =
                    ERhiTextureBindFlags::RenderTarget | ERhiTextureBindFlags::CopyDst;

                VkImageViewCreateInfo viewInfo{};
                viewInfo.sType                           = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
                viewInfo.image                           = s.mImages[i];
                viewInfo.viewType                        = VK_IMAGE_VIEW_TYPE_2D;
                viewInfo.format                          = chosen.format;
                viewInfo.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
                viewInfo.subresourceRange.baseMipLevel   = 0;
                viewInfo.subresourceRange.levelCount     = 1;
                viewInfo.subresourceRange.baseArrayLayer = 0;
                viewInfo.subresourceRange.layerCount     = 1;

                VkImageView imageView = VK_NULL_HANDLE;
                if (vkCreateImageView(s.mDevice, &viewInfo, nullptr, &imageView) != VK_SUCCESS) {
                    imageView = VK_NULL_HANDLE;
                }

                s.mBackBuffers.PushBack(FRhiTextureRef::Adopt(
                    new FRhiVulkanTexture(texDesc, s.mDevice, s.mImages[i], imageView, false)));
            }

            // Ensure semaphore arrays are at least imageCount (used as the ring size).
            if (s.mAcquireSemaphores.Size() < imageCount) {
                const usize old = s.mAcquireSemaphores.Size();
                s.mAcquireSemaphores.Resize(imageCount);
                s.mRenderCompleteSemaphores.Resize(imageCount);
                s.mAcquireFences.Resize(imageCount);
                for (usize i = old; i < imageCount; ++i) {
                    s.mAcquireSemaphores[i]        = VK_NULL_HANDLE;
                    s.mRenderCompleteSemaphores[i] = VK_NULL_HANDLE;
                    s.mAcquireFences[i]            = VK_NULL_HANDLE;
                }
            }
            for (usize i = 0; i < s.mAcquireSemaphores.Size(); ++i) {
                if (!s.mAcquireSemaphores[i]) {
                    s.mAcquireSemaphores[i] = CreateBinarySemaphore(s.mDevice);
                }
                if (!s.mRenderCompleteSemaphores[i]) {
                    s.mRenderCompleteSemaphores[i] = CreateBinarySemaphore(s.mDevice);
                }
                if (!s.mAcquireFences[i]) {
                    VkFenceCreateInfo fenceInfo{};
                    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
                    // First reuse must not block.
                    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;
                    if (vkCreateFence(s.mDevice, &fenceInfo, nullptr, &s.mAcquireFences[i])
                        != VK_SUCCESS) {
                        s.mAcquireFences[i] = VK_NULL_HANDLE;
                    }
                }
            }

            s.mAcquired   = false;
            s.mImageIndex = 0U;
            return true;
        };

#if AE_PLATFORM_WIN
        HWND hwnd = reinterpret_cast<HWND>(desc.mNativeHandle);
        if (hwnd == nullptr) {
            LogError(TEXT("RHI(Vulkan): Viewport requires a valid HWND."));
            return;
        }

        VkWin32SurfaceCreateInfoKHR surfaceInfo{};
        surfaceInfo.sType     = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
        surfaceInfo.hinstance = GetModuleHandleW(nullptr);
        surfaceInfo.hwnd      = hwnd;

        if (vkCreateWin32SurfaceKHR(instance, &surfaceInfo, nullptr, &mState->mSurface)
            != VK_SUCCESS) {
            LogError(TEXT("RHI(Vulkan): Failed to create Win32 surface."));
            return;
        }

        if (!createSwapchain(desc)) {
            LogError(TEXT("RHI(Vulkan): Failed to create swapchain."));
        }
#else
        (void)desc;
#endif
    }

    FRhiVulkanViewport::~FRhiVulkanViewport() {
        if (!mState) {
            return;
        }

        WaitViewportGpuIdle(mState->mOwner, mState->mGraphicsQueue);

        mState->mBackBuffers.Clear();
        mState->mImages.Clear();
        if (mState->mDevice && mState->mSwapchain) {
            vkDestroySwapchainKHR(mState->mDevice, mState->mSwapchain, nullptr);
            mState->mSwapchain = VK_NULL_HANDLE;
        }

        if (mState->mDevice) {
            for (VkSemaphore sem : mState->mAcquireSemaphores) {
                if (sem) {
                    vkDestroySemaphore(mState->mDevice, sem, nullptr);
                }
            }
            for (VkSemaphore sem : mState->mRenderCompleteSemaphores) {
                if (sem) {
                    vkDestroySemaphore(mState->mDevice, sem, nullptr);
                }
            }
            for (VkFence fence : mState->mAcquireFences) {
                if (fence) {
                    vkDestroyFence(mState->mDevice, fence, nullptr);
                }
            }
        }

        if (mState->mInstance && mState->mSurface) {
            vkDestroySurfaceKHR(mState->mInstance, mState->mSurface, nullptr);
            mState->mSurface = VK_NULL_HANDLE;
        }

        delete mState;
        mState = nullptr;
    }

    void FRhiVulkanViewport::Resize(u32 width, u32 height) {
        UpdateExtent(width, height);
        if (!mState) {
            return;
        }

        WaitViewportGpuIdle(mState->mOwner, mState->mGraphicsQueue);

        mState->mBackBuffers.Clear();
        mState->mImages.Clear();
        if (mState->mDevice && mState->mSwapchain) {
            vkDestroySwapchainKHR(mState->mDevice, mState->mSwapchain, nullptr);
            mState->mSwapchain = VK_NULL_HANDLE;
        }

        FRhiViewportDesc desc = GetDesc();
        desc.mWidth           = width;
        desc.mHeight          = height;

        // Recreate swapchain (duplicated from ctor to keep FState private).
        VkSurfaceCapabilitiesKHR caps{};
        if (vkGetPhysicalDeviceSurfaceCapabilitiesKHR(
                mState->mPhysicalDevice, mState->mSurface, &caps)
            != VK_SUCCESS) {
            return;
        }

        u32 formatCount = 0;
        vkGetPhysicalDeviceSurfaceFormatsKHR(
            mState->mPhysicalDevice, mState->mSurface, &formatCount, nullptr);
        TVector<VkSurfaceFormatKHR> formats;
        formats.Resize(formatCount);
        if (formatCount > 0) {
            vkGetPhysicalDeviceSurfaceFormatsKHR(
                mState->mPhysicalDevice, mState->mSurface, &formatCount, formats.Data());
        }

        VkSurfaceFormatKHR chosen{};
        chosen.format     = Vulkan::Detail::ToVkFormat(desc.mFormat);
        chosen.colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
        bool found        = false;
        for (const auto& f : formats) {
            if (f.format == chosen.format) {
                chosen = f;
                found  = true;
                break;
            }
        }
        if (!found && !formats.IsEmpty()) {
            chosen = formats[0];
        }
        mState->mFormat = chosen.format;

        VkPresentModeKHR presentMode =
            PickPresentMode(mState->mPhysicalDevice, mState->mSurface, desc.mAllowTearing);

        VkExtent2D extent{};
        extent.width  = desc.mWidth;
        extent.height = desc.mHeight;
        if (caps.currentExtent.width != UINT32_MAX) {
            extent = caps.currentExtent;
        } else {
            extent.width = Core::Math::Clamp(
                extent.width, caps.minImageExtent.width, caps.maxImageExtent.width);
            extent.height = Core::Math::Clamp(
                extent.height, caps.minImageExtent.height, caps.maxImageExtent.height);
        }
        mState->mExtent = extent;

        u32 imageCount = desc.mBufferCount;
        if (imageCount < caps.minImageCount) {
            imageCount = caps.minImageCount;
        }
        if (caps.maxImageCount > 0 && imageCount > caps.maxImageCount) {
            imageCount = caps.maxImageCount;
        }

        VkSwapchainCreateInfoKHR info{};
        info.sType            = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
        info.surface          = mState->mSurface;
        info.minImageCount    = imageCount;
        info.imageFormat      = chosen.format;
        info.imageColorSpace  = chosen.colorSpace;
        info.imageExtent      = extent;
        info.imageArrayLayers = 1;
        info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
        info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
        info.preTransform     = caps.currentTransform;
        info.compositeAlpha   = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
        info.presentMode      = presentMode;
        info.clipped          = VK_TRUE;
        info.oldSwapchain     = VK_NULL_HANDLE;

        if (vkCreateSwapchainKHR(mState->mDevice, &info, nullptr, &mState->mSwapchain)
            != VK_SUCCESS) {
            mState->mSwapchain = VK_NULL_HANDLE;
            return;
        }

        u32 outCount = 0;
        vkGetSwapchainImagesKHR(mState->mDevice, mState->mSwapchain, &outCount, nullptr);
        mState->mImages.Resize(outCount);
        vkGetSwapchainImagesKHR(
            mState->mDevice, mState->mSwapchain, &outCount, mState->mImages.Data());

        mState->mBackBuffers.Reserve(outCount);
        for (u32 i = 0; i < outCount; ++i) {
            FRhiTextureDesc texDesc{};
            texDesc.mDebugName = desc.mDebugName;
            texDesc.mWidth     = extent.width;
            texDesc.mHeight    = extent.height;
            texDesc.mFormat    = desc.mFormat;
            texDesc.mBindFlags = ERhiTextureBindFlags::RenderTarget | ERhiTextureBindFlags::CopyDst;

            VkImageViewCreateInfo viewInfo{};
            viewInfo.sType                           = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
            viewInfo.image                           = mState->mImages[i];
            viewInfo.viewType                        = VK_IMAGE_VIEW_TYPE_2D;
            viewInfo.format                          = chosen.format;
            viewInfo.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
            viewInfo.subresourceRange.baseMipLevel   = 0;
            viewInfo.subresourceRange.levelCount     = 1;
            viewInfo.subresourceRange.baseArrayLayer = 0;
            viewInfo.subresourceRange.layerCount     = 1;

            VkImageView imageView = VK_NULL_HANDLE;
            if (vkCreateImageView(mState->mDevice, &viewInfo, nullptr, &imageView) != VK_SUCCESS) {
                imageView = VK_NULL_HANDLE;
            }

            mState->mBackBuffers.PushBack(FRhiTextureRef::Adopt(new FRhiVulkanTexture(
                texDesc, mState->mDevice, mState->mImages[i], imageView, false)));
        }

        if (mState->mAcquireSemaphores.Size() < imageCount) {
            const usize old = mState->mAcquireSemaphores.Size();
            mState->mAcquireSemaphores.Resize(imageCount);
            mState->mRenderCompleteSemaphores.Resize(imageCount);
            mState->mAcquireFences.Resize(imageCount);
            for (usize i = old; i < imageCount; ++i) {
                mState->mAcquireSemaphores[i]        = VK_NULL_HANDLE;
                mState->mRenderCompleteSemaphores[i] = VK_NULL_HANDLE;
                mState->mAcquireFences[i]            = VK_NULL_HANDLE;
            }
        }
        for (usize i = 0; i < mState->mAcquireSemaphores.Size(); ++i) {
            if (!mState->mAcquireSemaphores[i]) {
                mState->mAcquireSemaphores[i] = CreateBinarySemaphore(mState->mDevice);
            }
            if (!mState->mRenderCompleteSemaphores[i]) {
                mState->mRenderCompleteSemaphores[i] = CreateBinarySemaphore(mState->mDevice);
            }
            if (!mState->mAcquireFences[i]) {
                VkFenceCreateInfo fenceInfo{};
                fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
                fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;
                if (vkCreateFence(mState->mDevice, &fenceInfo, nullptr, &mState->mAcquireFences[i])
                    != VK_SUCCESS) {
                    mState->mAcquireFences[i] = VK_NULL_HANDLE;
                }
            }
        }

        mState->mAcquired   = false;
        mState->mImageIndex = 0U;
    }

    auto FRhiVulkanViewport::GetBackBuffer() const noexcept -> FRhiTexture* {
        if (!mState || !mState->mSwapchain || mState->mImages.IsEmpty()) {
            return nullptr;
        }

        if (!mState->mAcquired) {
            auto AcquireSwapchainImage = [this]() noexcept -> VkResult {
                if (mState->mAcquireSemaphores.IsEmpty() || mState->mAcquireFences.IsEmpty()) {
                    return VK_ERROR_INITIALIZATION_FAILED;
                }

                const usize ringIndex =
                    static_cast<usize>(mState->mFrameIndex % mState->mAcquireSemaphores.Size());
                VkFence fence = mState->mAcquireFences[ringIndex];
                if (fence != VK_NULL_HANDLE) {
                    const VkResult waitBeforeAcquire =
                        vkWaitForFences(mState->mDevice, 1, &fence, VK_TRUE, kAcquireTimeoutNs);
                    if (waitBeforeAcquire != VK_SUCCESS) {
                        return waitBeforeAcquire;
                    }
                    const VkResult resetBeforeAcquire = vkResetFences(mState->mDevice, 1, &fence);
                    if (resetBeforeAcquire != VK_SUCCESS) {
                        return resetBeforeAcquire;
                    }
                }

                VkResult acquireResult = VK_ERROR_UNKNOWN;
                {
                    // Serialize swapchain API usage with submit/present thread.
                    FQueueApiScopedLock lock;
                    acquireResult = vkAcquireNextImageKHR(mState->mDevice, mState->mSwapchain,
                        kAcquireTimeoutNs, VK_NULL_HANDLE, fence, &mState->mImageIndex);
                }
                if (acquireResult != VK_SUCCESS && acquireResult != VK_SUBOPTIMAL_KHR) {
                    return acquireResult;
                }
                if (fence != VK_NULL_HANDLE) {
                    const VkResult waitResult =
                        vkWaitForFences(mState->mDevice, 1, &fence, VK_TRUE, kAcquireTimeoutNs);
                    if (waitResult != VK_SUCCESS) {
                        return waitResult;
                    }
                }

                VkSemaphore renderComplete = (mState->mImageIndex < static_cast<u32>(
                                                  mState->mRenderCompleteSemaphores.Size()))
                    ? mState->mRenderCompleteSemaphores[mState->mImageIndex]
                    : VK_NULL_HANDLE;
                if (mState->mOwner) {
                    // Acquire synchronization is completed on CPU fence wait above.
                    mState->mOwner->NotifyViewportAcquired(VK_NULL_HANDLE, renderComplete);
                }

                mState->mAcquired = true;
                return acquireResult;
            };

            VkResult acquireResult = AcquireSwapchainImage();
            if (IsSwapchainOutOfDate(acquireResult)) {
                auto*      self = const_cast<FRhiVulkanViewport*>(this);
                const auto desc = self->GetDesc();
                LogWarningCat(TEXT("RHI.Vulkan"),
                    "GetBackBuffer: swapchain out-of-date during acquire (result={}), recreate and retry.",
                    static_cast<int>(acquireResult));
                self->Resize(desc.mWidth, desc.mHeight);
                if (!mState || !mState->mSwapchain || mState->mImages.IsEmpty()) {
                    return nullptr;
                }
                acquireResult = AcquireSwapchainImage();
            }

            if (acquireResult != VK_SUCCESS && acquireResult != VK_SUBOPTIMAL_KHR) {
                LogWarningCat(TEXT("RHI.Vulkan"),
                    "GetBackBuffer: vkAcquireNextImageKHR failed (result={}).",
                    static_cast<int>(acquireResult));
                return nullptr;
            }
        }

        if (mState->mImageIndex >= static_cast<u32>(mState->mBackBuffers.Size())) {
            return nullptr;
        }
        return mState->mBackBuffers[mState->mImageIndex].Get();
    }

    void FRhiVulkanViewport::Present(const FRhiPresentInfo& info) {
        (void)info;
        if (!mState) {
            return;
        }
        // Actual present is executed by the queue submit thread via FRhiVulkanQueue::Present.
        mState->mFrameIndex++;
        mState->mAcquired = false;
    }

    auto FRhiVulkanViewport::IsImageAcquired() const noexcept -> bool {
        return (mState != nullptr) ? mState->mAcquired : false;
    }

    auto FRhiVulkanViewport::GetNativeSwapchain() const noexcept -> VkSwapchainKHR {
        return (mState != nullptr) ? mState->mSwapchain : VK_NULL_HANDLE;
    }

    auto FRhiVulkanViewport::GetAcquireSemaphore() const noexcept -> VkSemaphore {
        (void)mState;
        return VK_NULL_HANDLE;
    }

    auto FRhiVulkanViewport::GetRenderCompleteSemaphore() const noexcept -> VkSemaphore {
        if (!mState || mState->mRenderCompleteSemaphores.IsEmpty() || !mState->mAcquired) {
            return VK_NULL_HANDLE;
        }
        if (mState->mImageIndex >= static_cast<u32>(mState->mRenderCompleteSemaphores.Size())) {
            return VK_NULL_HANDLE;
        }
        return mState->mRenderCompleteSemaphores[mState->mImageIndex];
    }

    auto FRhiVulkanViewport::GetCurrentImageIndex() const noexcept -> u32 {
        return (mState != nullptr) ? mState->mImageIndex : 0U;
    }

} // namespace AltinaEngine::Rhi
