#pragma once

#include "RhiVulkanAPI.h"
#include "Rhi/RhiCommandContext.h"
#include "Rhi/Command/Internal/RhiCommandSection.h"
#include "Rhi/Command/RhiCmdContextOps.h"
#include "Rhi/RhiRefs.h"

namespace AltinaEngine::Rhi {
    class FRhiVulkanDevice;
    class FRhiVulkanCommandList;

    class AE_RHI_VULKAN_API FRhiVulkanCommandContext final : public FRhiCommandContext {
    public:
        FRhiVulkanCommandContext(
            const FRhiCommandContextDesc& desc, VkDevice device, FRhiVulkanDevice* owner);
        ~FRhiVulkanCommandContext() override;

        auto RHISubmitActiveSection(const FRhiCommandContextSubmitInfo& submitInfo)
            -> FRhiCommandSubmissionStamp override;
        auto RHIFlushContextHost(const FRhiCommandContextSubmitInfo& submitInfo)
            -> FRhiCommandHostSyncPoint override;
        auto RHIFlushContextDevice(const FRhiCommandContextSubmitInfo& submitInfo)
            -> FRhiCommandSubmissionStamp override;
        auto RHISwitchContextCapability(ERhiContextCapability capability)
            -> FRhiCommandSubmissionStamp override;

        void RHIUpdateDynamicBufferDiscard(
            FRhiBuffer* buffer, const void* data, u64 sizeBytes, u64 offsetBytes) override;

        void RHISetGraphicsPipeline(FRhiPipeline* pipeline) override;
        void RHISetComputePipeline(FRhiPipeline* pipeline) override;
        void RHISetPrimitiveTopology(ERhiPrimitiveTopology topology) override;
        void RHISetVertexBuffer(u32 slot, const FRhiVertexBufferView& view) override;
        void RHISetIndexBuffer(const FRhiIndexBufferView& view) override;
        void RHISetViewport(const FRhiViewportRect& viewport) override;
        void RHISetScissor(const FRhiScissorRect& scissor) override;
        void RHISetRenderTargets(u32 colorTargetCount, FRhiTexture* const* colorTargets,
            FRhiTexture* depthTarget) override;
        void RHIBeginRenderPass(const FRhiRenderPassDesc& desc) override;
        void RHIEndRenderPass() override;
        void RHIBeginTransition(const FRhiTransitionCreateInfo& info) override;
        void RHIEndTransition(const FRhiTransitionCreateInfo& info) override;
        void RHIClearColor(FRhiTexture* colorTarget, const FRhiClearColor& color) override;
        void RHISetBindGroup(u32 setIndex, FRhiBindGroup* group, const u32* dynamicOffsets,
            u32 dynamicOffsetCount) override;
        void RHIDraw(
            u32 vertexCount, u32 instanceCount, u32 firstVertex, u32 firstInstance) override;
        void RHIDrawIndexed(u32 indexCount, u32 instanceCount, u32 firstIndex, i32 vertexOffset,
            u32 firstInstance) override;
        void RHIDispatch(u32 groupCountX, u32 groupCountY, u32 groupCountZ) override;

        [[nodiscard]] auto GetNativeCommandBuffer() const noexcept -> VkCommandBuffer;

    private:
        auto AcquireActiveSection() -> FRhiCommandSection*;
        void EnsureRecording();
        void FinalizeRecording();
        auto GetExecutionCommandList() const noexcept -> FRhiVulkanCommandList*;
        void RHIPushDebugMarkerNative(FStringView text) override;
        void RHIPopDebugMarkerNative() override;
        void RHIInsertDebugMarkerNative(FStringView text) override;

        struct FState;
        FState*                        mState = nullptr;
        FRhiCommandPoolRef             mPool;
        FRhiCommandSectionPool         mSectionPool;
        FRhiCommandSubmissionProcessor mSubmissionProcessor;
        FRhiCommandSection*            mActiveSection = nullptr;
        FRhiCommandSubmissionStamp     mLastStamp;
    };

} // namespace AltinaEngine::Rhi
