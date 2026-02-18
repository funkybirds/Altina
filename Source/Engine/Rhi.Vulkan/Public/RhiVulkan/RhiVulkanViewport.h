#pragma once

#include "RhiVulkanAPI.h"
#include "Rhi/RhiViewport.h"

#if defined(AE_RHI_VULKAN_AVAILABLE) && AE_RHI_VULKAN_AVAILABLE
    #include <vulkan/vulkan.h>
#else
    struct VkSurfaceKHR_T;
    struct VkSwapchainKHR_T;
    struct VkSemaphore_T;
    using VkSurfaceKHR = VkSurfaceKHR_T*;
    using VkSwapchainKHR = VkSwapchainKHR_T*;
    using VkSemaphore = VkSemaphore_T*;
#endif

namespace AltinaEngine::Rhi {
    class AE_RHI_VULKAN_API FRhiVulkanViewport final : public FRhiViewport {
    public:
        FRhiVulkanViewport(const FRhiViewportDesc& desc, VkInstance instance, VkDevice device,
            VkPhysicalDevice physicalDevice, VkQueue graphicsQueue, u32 graphicsQueueFamily);
        ~FRhiVulkanViewport() override;

        void Resize(u32 width, u32 height) override;
        [[nodiscard]] auto GetBackBuffer() const noexcept -> FRhiTexture* override;
        void Present(const FRhiPresentInfo& info) override;

        [[nodiscard]] auto GetNativeSwapchain() const noexcept -> VkSwapchainKHR;
        [[nodiscard]] auto GetAcquireSemaphore() const noexcept -> VkSemaphore;
        [[nodiscard]] auto GetRenderCompleteSemaphore() const noexcept -> VkSemaphore;
        [[nodiscard]] auto GetCurrentImageIndex() const noexcept -> u32;

    private:
        struct FState;
        FState* mState = nullptr;
    };

} // namespace AltinaEngine::Rhi
