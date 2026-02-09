#pragma once

#include "RhiGeneralAPI.h"
#include "Rhi/Command/RhiCmd.h"
#include "Rhi/RhiStructs.h"
#include "Types/Aliases.h"

namespace AltinaEngine::Rhi {
    class FRhiCmdDrawIndexed final : public FRhiCmd {
    public:
        FRhiCmdDrawIndexed(u32 indexCount, u32 instanceCount, u32 firstIndex,
            i32 vertexOffset, u32 firstInstance)
            : mIndexCount(indexCount)
            , mInstanceCount(instanceCount)
            , mFirstIndex(firstIndex)
            , mVertexOffset(vertexOffset)
            , mFirstInstance(firstInstance) {}

        void Execute(FRhiCmdContext& context) override {
            context.RHIDrawIndexed(mIndexCount, mInstanceCount, mFirstIndex,
                mVertexOffset, mFirstInstance);
        }

    private:
        u32 mIndexCount = 0U;
        u32 mInstanceCount = 0U;
        u32 mFirstIndex = 0U;
        i32 mVertexOffset = 0;
        u32 mFirstInstance = 0U;
    };

    class FRhiCmdDraw final : public FRhiCmd {
    public:
        FRhiCmdDraw(u32 vertexCount, u32 instanceCount, u32 firstVertex, u32 firstInstance)
            : mVertexCount(vertexCount),
              mInstanceCount(instanceCount),
              mFirstVertex(firstVertex),
              mFirstInstance(firstInstance) {}

        void Execute(FRhiCmdContext& context) override {
            context.RHIDraw(mVertexCount, mInstanceCount, mFirstVertex, mFirstInstance);
        }

    private:
        u32 mVertexCount = 0U;
        u32 mInstanceCount = 0U;
        u32 mFirstVertex = 0U;
        u32 mFirstInstance = 0U;
    };

    class FRhiCmdSetIndexBuffer final : public FRhiCmd {
    public:
        explicit FRhiCmdSetIndexBuffer(const FRhiIndexBufferView& view) : mView(view) {}

        void Execute(FRhiCmdContext& context) override { context.RHISetIndexBuffer(mView); }

    private:
        FRhiIndexBufferView mView{};
    };

    class FRhiCmdSetVertexBuffer final : public FRhiCmd {
    public:
        FRhiCmdSetVertexBuffer(u32 slot, const FRhiVertexBufferView& view)
            : mSlot(slot), mView(view) {}

        void Execute(FRhiCmdContext& context) override { context.RHISetVertexBuffer(mSlot, mView); }

    private:
        u32 mSlot = 0U;
        FRhiVertexBufferView mView{};
    };

    class FRhiCmdSetPrimitiveTopology final : public FRhiCmd {
    public:
        explicit FRhiCmdSetPrimitiveTopology(ERhiPrimitiveTopology topology)
            : mTopology(topology) {}

        void Execute(FRhiCmdContext& context) override { context.RHISetPrimitiveTopology(mTopology); }

    private:
        ERhiPrimitiveTopology mTopology = ERhiPrimitiveTopology::TriangleList;
    };

    class FRhiCmdSetViewport final : public FRhiCmd {
    public:
        explicit FRhiCmdSetViewport(const FRhiViewportRect& viewport) : mViewport(viewport) {}

        void Execute(FRhiCmdContext& context) override { context.RHISetViewport(mViewport); }

    private:
        FRhiViewportRect mViewport{};
    };

    class FRhiCmdSetScissor final : public FRhiCmd {
    public:
        explicit FRhiCmdSetScissor(const FRhiScissorRect& scissor) : mScissor(scissor) {}

        void Execute(FRhiCmdContext& context) override { context.RHISetScissor(mScissor); }

    private:
        FRhiScissorRect mScissor{};
    };

    class FRhiCmdClearColor final : public FRhiCmd {
    public:
        FRhiCmdClearColor(FRhiTexture* target, const FRhiClearColor& color)
            : mTarget(target), mColor(color) {}

        void Execute(FRhiCmdContext& context) override { context.RHIClearColor(mTarget, mColor); }

    private:
        FRhiTexture* mTarget = nullptr;
        FRhiClearColor mColor{};
    };

    class FRhiCmdSetRenderTargets final : public FRhiCmd {
    public:
        FRhiCmdSetRenderTargets(u32 colorTargetCount, FRhiTexture* const* colorTargets,
            FRhiTexture* depthTarget)
            : mColorTargetCount(colorTargetCount),
              mColorTargets(colorTargets),
              mDepthTarget(depthTarget) {}

        void Execute(FRhiCmdContext& context) override {
            context.RHISetRenderTargets(mColorTargetCount, mColorTargets, mDepthTarget);
        }

    private:
        u32 mColorTargetCount = 0U;
        FRhiTexture* const* mColorTargets = nullptr;
        FRhiTexture* mDepthTarget = nullptr;
    };

    class FRhiCmdBeginRenderPass final : public FRhiCmd {
    public:
        explicit FRhiCmdBeginRenderPass(const FRhiRenderPassDesc& desc) : mDesc(desc) {}

        void Execute(FRhiCmdContext& context) override { context.RHIBeginRenderPass(mDesc); }

    private:
        FRhiRenderPassDesc mDesc{};
    };

    class FRhiCmdEndRenderPass final : public FRhiCmd {
    public:
        FRhiCmdEndRenderPass() = default;

        void Execute(FRhiCmdContext& context) override { context.RHIEndRenderPass(); }
    };

    class FRhiCmdSetBindGroup final : public FRhiCmd {
    public:
        FRhiCmdSetBindGroup(u32 setIndex, FRhiBindGroup* group, const u32* dynamicOffsets,
            u32 dynamicOffsetCount)
            : mSetIndex(setIndex),
              mGroup(group),
              mDynamicOffsets(dynamicOffsets),
              mDynamicOffsetCount(dynamicOffsetCount) {}

        void Execute(FRhiCmdContext& context) override {
            context.RHISetBindGroup(mSetIndex, mGroup, mDynamicOffsets, mDynamicOffsetCount);
        }

    private:
        u32 mSetIndex = 0U;
        FRhiBindGroup* mGroup = nullptr;
        const u32* mDynamicOffsets = nullptr;
        u32 mDynamicOffsetCount = 0U;
    };

    class FRhiCmdDispatch final : public FRhiCmd {
    public:
        FRhiCmdDispatch(u32 groupCountX, u32 groupCountY, u32 groupCountZ)
            : mGroupCountX(groupCountX), mGroupCountY(groupCountY), mGroupCountZ(groupCountZ) {}

        void Execute(FRhiCmdContext& context) override {
            context.RHIDispatch(mGroupCountX, mGroupCountY, mGroupCountZ);
        }

    private:
        u32 mGroupCountX = 1U;
        u32 mGroupCountY = 1U;
        u32 mGroupCountZ = 1U;
    };

} // namespace AltinaEngine::Rhi
