#include "TestHarness.h"

#include "Rhi/Command/RhiCmdContext.h"
#include "Rhi/RhiBuffer.h"

namespace {
    using AltinaEngine::i32;
    using AltinaEngine::u32;
    using AltinaEngine::u64;
    using AltinaEngine::Rhi::ERhiBufferBindFlags;
    using AltinaEngine::Rhi::ERhiPrimitiveTopology;
    using AltinaEngine::Rhi::FRhiBindGroup;
    using AltinaEngine::Rhi::FRhiBuffer;
    using AltinaEngine::Rhi::FRhiBufferDesc;
    using AltinaEngine::Rhi::FRhiClearColor;
    using AltinaEngine::Rhi::FRhiCmdContext;
    using AltinaEngine::Rhi::FRhiIndexBufferView;
    using AltinaEngine::Rhi::FRhiPipeline;
    using AltinaEngine::Rhi::FRhiRenderPassDesc;
    using AltinaEngine::Rhi::FRhiScissorRect;
    using AltinaEngine::Rhi::FRhiTexture;
    using AltinaEngine::Rhi::FRhiTransitionCreateInfo;
    using AltinaEngine::Rhi::FRhiVertexBufferView;
    using AltinaEngine::Rhi::FRhiViewportRect;

    class FTestBuffer final : public FRhiBuffer {
    public:
        explicit FTestBuffer(const FRhiBufferDesc& desc) : FRhiBuffer(desc) {}
    };

    class FVertexBindingFallbackContext final : public FRhiCmdContext {
    public:
        void RHIUpdateDynamicBufferDiscard(FRhiBuffer* /*buffer*/, const void* /*data*/,
            u64 /*sizeBytes*/, u64 /*offsetBytes*/) override {}
        void RHISetGraphicsPipeline(FRhiPipeline* /*pipeline*/) override {}
        void RHISetComputePipeline(FRhiPipeline* /*pipeline*/) override {}
        void RHISetPrimitiveTopology(ERhiPrimitiveTopology /*topology*/) override {}
        void RHISetVertexBuffer(u32 slot, const FRhiVertexBufferView& view) override {
            ++mSingleBindCount;
            mLastSlot   = slot;
            mLastBuffer = view.mBuffer;
        }
        void RHISetIndexBuffer(const FRhiIndexBufferView& /*view*/) override {}
        void RHISetViewport(const FRhiViewportRect& /*viewport*/) override {}
        void RHISetScissor(const FRhiScissorRect& /*scissor*/) override {}
        void RHISetRenderTargets(u32 /*colorTargetCount*/, FRhiTexture* const* /*colorTargets*/,
            FRhiTexture* /*depthTarget*/) override {}
        void RHIBeginRenderPass(const FRhiRenderPassDesc& /*desc*/) override {}
        void RHIEndRenderPass() override {}
        void RHIBeginTransition(const FRhiTransitionCreateInfo& /*info*/) override {}
        void RHIEndTransition(const FRhiTransitionCreateInfo& /*info*/) override {}
        void RHIClearColor(FRhiTexture* /*colorTarget*/, const FRhiClearColor& /*color*/) override {
        }
        void RHISetBindGroup(u32 /*setIndex*/, FRhiBindGroup* /*group*/,
            const u32* /*dynamicOffsets*/, u32 /*dynamicOffsetCount*/) override {}
        void RHIDraw(u32 /*vertexCount*/, u32 /*instanceCount*/, u32 /*firstVertex*/,
            u32 /*firstInstance*/) override {}
        void RHIDrawIndexed(u32 /*indexCount*/, u32 /*instanceCount*/, u32 /*firstIndex*/,
            i32 /*vertexOffset*/, u32 /*firstInstance*/) override {}
        void RHIDispatch(u32 /*groupCountX*/, u32 /*groupCountY*/, u32 /*groupCountZ*/) override {}

        u32  mSingleBindCount   = 0U;
        u32  mLastSlot          = 0U;
        FRhiBuffer* mLastBuffer = nullptr;
    };
} // namespace

TEST_CASE("Rhi.VertexBufferBatchFallbackDispatchesSingleSlotBinds") {
    FRhiBufferDesc bufferDesc{};
    bufferDesc.mSizeBytes = 64ULL;
    bufferDesc.mBindFlags = ERhiBufferBindFlags::Vertex;
    FTestBuffer          bufferA(bufferDesc);
    FTestBuffer          bufferB(bufferDesc);
    FTestBuffer          bufferC(bufferDesc);

    FRhiVertexBufferView views[3]{};
    views[0].mBuffer      = &bufferA;
    views[0].mStrideBytes = 12U;
    views[1].mBuffer      = &bufferB;
    views[1].mStrideBytes = 12U;
    views[2].mBuffer      = &bufferC;
    views[2].mStrideBytes = 8U;

    FVertexBindingFallbackContext context{};
    context.RHISetVertexBuffers(2U, views, 3U);

    REQUIRE_EQ(context.mSingleBindCount, 3U);
    REQUIRE_EQ(context.mLastSlot, 4U);
    REQUIRE(context.mLastBuffer == &bufferC);
}
