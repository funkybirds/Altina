#pragma once

#include "RhiVulkanAPI.h"
#include "Rhi/RhiCommandList.h"
#include "Container/Vector.h"
#include "RhiVulkan/RhiVulkanResources.h"

namespace AltinaEngine::Rhi {
    namespace Container = Core::Container;
    using Container::TVector;

    class AE_RHI_VULKAN_API FRhiVulkanCommandList final : public FRhiCommandList {
    public:
        explicit FRhiVulkanCommandList(const FRhiCommandListDesc& desc);
        ~FRhiVulkanCommandList() override;

        [[nodiscard]] auto GetNativeCommandBuffer() const noexcept -> VkCommandBuffer;

        void               Reset(FRhiCommandPool* pool) override;
        void               Close() override;

        void               AddTouchedTexture(FRhiVulkanTexture* texture);
        [[nodiscard]] auto GetTouchedTextures() const noexcept
            -> const TVector<FRhiVulkanTexture*>&;
        void ClearTouchedTextures();

    private:
        void SetNativeCommandBuffer(VkCommandBuffer buffer);

        struct FState;
        FState* mState = nullptr;

        friend class FRhiVulkanCommandContext;
    };

} // namespace AltinaEngine::Rhi
