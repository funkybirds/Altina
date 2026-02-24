#pragma once

#include "RhiVulkanAPI.h"
#include "Rhi/RhiCommandList.h"

namespace AltinaEngine::Rhi {
    class AE_RHI_VULKAN_API FRhiVulkanCommandList final : public FRhiCommandList {
    public:
        explicit FRhiVulkanCommandList(const FRhiCommandListDesc& desc);
        ~FRhiVulkanCommandList() override;

        [[nodiscard]] auto GetNativeCommandBuffer() const noexcept -> VkCommandBuffer;

        void               Reset(FRhiCommandPool* pool) override;
        void               Close() override;

    private:
        void SetNativeCommandBuffer(VkCommandBuffer buffer);

        struct FState;
        FState* mState = nullptr;

        friend class FRhiVulkanCommandContext;
    };

} // namespace AltinaEngine::Rhi
