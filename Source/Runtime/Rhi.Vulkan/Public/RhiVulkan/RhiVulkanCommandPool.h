#pragma once

#include "RhiVulkanAPI.h"
#include "Rhi/RhiCommandPool.h"

namespace AltinaEngine::Rhi {
    class AE_RHI_VULKAN_API FRhiVulkanCommandPool final : public FRhiCommandPool {
    public:
        FRhiVulkanCommandPool(
            const FRhiCommandPoolDesc& desc, VkDevice device, u32 queueFamilyIndex, bool transient);
        ~FRhiVulkanCommandPool() override;

        [[nodiscard]] auto GetNativePool() const noexcept -> VkCommandPool;
        [[nodiscard]] auto GetQueueFamilyIndex() const noexcept -> u32;

        void               Reset() override;

    private:
        struct FState;
        FState* mState = nullptr;
    };

} // namespace AltinaEngine::Rhi
