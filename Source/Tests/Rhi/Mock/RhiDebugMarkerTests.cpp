#include "TestHarness.h"

#include "Rhi/RhiCommandContext.h"
#include "Rhi/RhiDebugMarker.h"
#include "Container/String.h"
#include "Container/Vector.h"

namespace {
    using AltinaEngine::i32;
    using AltinaEngine::u32;
    using AltinaEngine::u64;
    using AltinaEngine::usize;
    using AltinaEngine::Core::Container::FString;
    using AltinaEngine::Core::Container::TVector;
    using AltinaEngine::Rhi::ERhiContextCapability;
    using AltinaEngine::Rhi::ERhiPrimitiveTopology;
    using AltinaEngine::Rhi::FRhiBindGroup;
    using AltinaEngine::Rhi::FRhiBuffer;
    using AltinaEngine::Rhi::FRhiClearColor;
    using AltinaEngine::Rhi::FRhiCommandContext;
    using AltinaEngine::Rhi::FRhiCommandContextDesc;
    using AltinaEngine::Rhi::FRhiCommandContextSubmitInfo;
    using AltinaEngine::Rhi::FRhiCommandHostSyncPoint;
    using AltinaEngine::Rhi::FRhiCommandSubmissionStamp;
    using AltinaEngine::Rhi::FRhiDebugMarker;
    using AltinaEngine::Rhi::FRhiIndexBufferView;
    using AltinaEngine::Rhi::FRhiPipeline;
    using AltinaEngine::Rhi::FRhiRenderPassDesc;
    using AltinaEngine::Rhi::FRhiScissorRect;
    using AltinaEngine::Rhi::FRhiTexture;
    using AltinaEngine::Rhi::FRhiTransitionCreateInfo;
    using AltinaEngine::Rhi::FRhiVertexBufferView;
    using AltinaEngine::Rhi::FRhiViewportRect;

    class FDebugMarkerTestContext final : public FRhiCommandContext {
    public:
        FDebugMarkerTestContext() : FRhiCommandContext(MakeDesc()) {}

        void BeginSection() { RHIBeginSectionDebugMarkers(); }
        void EndSection() { RHIEndSectionDebugMarkers(); }

        auto RHISubmitActiveSection(const FRhiCommandContextSubmitInfo& /*submitInfo*/)
            -> FRhiCommandSubmissionStamp override {
            return {};
        }
        auto RHIFlushContextHost(const FRhiCommandContextSubmitInfo& /*submitInfo*/)
            -> FRhiCommandHostSyncPoint override {
            return {};
        }
        auto RHIFlushContextDevice(const FRhiCommandContextSubmitInfo& /*submitInfo*/)
            -> FRhiCommandSubmissionStamp override {
            return {};
        }
        auto RHISwitchContextCapability(ERhiContextCapability /*capability*/)
            -> FRhiCommandSubmissionStamp override {
            return {};
        }

        void RHIUpdateDynamicBufferDiscard(FRhiBuffer* /*buffer*/, const void* /*data*/,
            u64 /*sizeBytes*/, u64 /*offsetBytes*/) override {}
        void RHISetGraphicsPipeline(FRhiPipeline* /*pipeline*/) override {}
        void RHISetComputePipeline(FRhiPipeline* /*pipeline*/) override {}
        void RHISetPrimitiveTopology(ERhiPrimitiveTopology /*topology*/) override {}
        void RHISetVertexBuffer(u32 /*slot*/, const FRhiVertexBufferView& /*view*/) override {}
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

        TVector<FString> mPushes;
        u32              mPopCount = 0U;
        TVector<FString> mInsertions;

    protected:
        void RHIPushDebugMarkerNative(AltinaEngine::Rhi::FStringView text) override {
            FString value;
            value.Append(text.Data(), text.Length());
            mPushes.PushBack(AltinaEngine::Move(value));
        }

        void RHIPopDebugMarkerNative() override { ++mPopCount; }

        void RHIInsertDebugMarkerNative(AltinaEngine::Rhi::FStringView text) override {
            FString value;
            value.Append(text.Data(), text.Length());
            mInsertions.PushBack(AltinaEngine::Move(value));
        }

    private:
        static auto MakeDesc() -> FRhiCommandContextDesc {
            FRhiCommandContextDesc desc{};
            desc.mDebugName.Assign(TEXT("DebugMarkerTest"));
            return desc;
        }
    };
} // namespace

TEST_CASE("RhiDebugMarker.ScopeReplaysAcrossSections") {
    FDebugMarkerTestContext ctx;
    ctx.RHIPushDebugMarker(TEXT("Frame"));

    ctx.BeginSection();
    ctx.RHIInsertDebugMarker(TEXT("InSection0"));
    ctx.RHIPushDebugMarker(TEXT("Pass"));
    ctx.EndSection();

    ctx.BeginSection();
    ctx.RHIPopDebugMarker();
    ctx.EndSection();
    ctx.RHIPopDebugMarker();

    REQUIRE_EQ(ctx.mPushes.Size(), static_cast<usize>(4));
    REQUIRE_EQ(ctx.mPushes[0], FString(TEXT("Frame")));
    REQUIRE_EQ(ctx.mPushes[1], FString(TEXT("Pass")));
    REQUIRE_EQ(ctx.mPushes[2], FString(TEXT("Frame")));
    REQUIRE_EQ(ctx.mPushes[3], FString(TEXT("Pass")));
    REQUIRE_EQ(ctx.mPopCount, 4U);
    REQUIRE_EQ(ctx.mInsertions.Size(), static_cast<usize>(1));
    REQUIRE_EQ(ctx.mInsertions[0], FString(TEXT("InSection0")));
}

TEST_CASE("RhiDebugMarker.RaiiPopsAtScopeEnd") {
    FDebugMarkerTestContext ctx;
    ctx.BeginSection();
    {
        FRhiDebugMarker marker(ctx, TEXT("Scoped"));
        ctx.RHIInsertDebugMarker(TEXT("Inner"));
    }
    ctx.EndSection();

    REQUIRE_EQ(ctx.mPushes.Size(), static_cast<usize>(1));
    REQUIRE_EQ(ctx.mPushes[0], FString(TEXT("Scoped")));
    REQUIRE_EQ(ctx.mPopCount, 1U);
    REQUIRE_EQ(ctx.mInsertions.Size(), static_cast<usize>(1));
    REQUIRE_EQ(ctx.mInsertions[0], FString(TEXT("Inner")));
}
