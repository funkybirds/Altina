#pragma once

#include "RhiD3D11API.h"
#include "Rhi/RhiCommandContext.h"
#include "Rhi/Command/Internal/RhiCommandSection.h"
#include "Rhi/Command/RhiCmdContextOps.h"
#include "Rhi/RhiRefs.h"

struct ID3D11Device;
struct ID3D11DeviceContext;

namespace AltinaEngine::Rhi {
    class FRhiD3D11Device;
    class FRhiD3D11CommandList;

    class AE_RHI_D3D11_API FRhiD3D11CommandContext final : public FRhiCommandContext {
    public:
        FRhiD3D11CommandContext(
            const FRhiCommandContextDesc& desc, ID3D11Device* device, FRhiD3D11Device* owner);
        ~FRhiD3D11CommandContext() override;

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

        [[nodiscard]] auto GetDeferredContext() noexcept -> ID3D11DeviceContext*;

        // Implementation state (defined in the .cpp). Exposed as an incomplete type so translation
        // units in this module can reference it without violating access control.
        struct FState;

    private:
        auto                   AcquireActiveSection() -> FRhiCommandSection*;
        void                   EnsureRecording();
        void                   FinalizeRecording();
        auto                   GetExecutionCommandList() const noexcept -> FRhiD3D11CommandList*;
        void                   RHIPushDebugMarkerNative(FStringView text) override;
        void                   RHIPopDebugMarkerNative() override;
        void                   RHIInsertDebugMarkerNative(FStringView text) override;

        FState*                mState = nullptr;
        FRhiD3D11Device*       mOwner = nullptr;
        FRhiCommandSectionPool mSectionPool;
        FRhiCommandSubmissionProcessor mSubmissionProcessor;
        FRhiCommandSection*            mActiveSection = nullptr;
        FRhiCommandSubmissionStamp     mLastStamp;
    };

} // namespace AltinaEngine::Rhi
