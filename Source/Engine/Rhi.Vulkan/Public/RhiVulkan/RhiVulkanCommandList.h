#pragma once

#include "RhiVulkanAPI.h"
#include "Rhi/RhiCommandList.h"

#if defined(AE_RHI_VULKAN_AVAILABLE) && AE_RHI_VULKAN_AVAILABLE
    #include <vulkan/vulkan.h>
#else
    struct VkCommandBuffer_T;
    using VkCommandBuffer = VkCommandBuffer_T*;
#endif

namespace AltinaEngine::Rhi {
    class AE_RHI_VULKAN_API FRhiVulkanCommandList final : public FRhiCommandList {
    public:
        explicit FRhiVulkanCommandList(const FRhiCommandListDesc& desc);
        ~FRhiVulkanCommandList() override;

        [[nodiscard]] auto GetNativeCommandBuffer() const noexcept -> VkCommandBuffer;

        void Reset(FRhiCommandPool* pool) override;
        void Close() override;

    private:
        void SetNativeCommandBuffer(VkCommandBuffer buffer);

        struct FState;
        FState* mState = nullptr;

        friend class FRhiVulkanCommandContext;
    };

} // namespace AltinaEngine::Rhi
