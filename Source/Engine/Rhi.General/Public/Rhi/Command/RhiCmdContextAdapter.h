#pragma once

#include "RhiGeneralAPI.h"
#include "Rhi/Command/RhiCmdContext.h"
#include "Rhi/RhiCommandContext.h"
#include "Types/Traits.h"

namespace AltinaEngine::Rhi {
    class FRhiCmdContextAdapter final : public FRhiCmdContext {
    public:
        template <typename TContext>
            requires AltinaEngine::CClassBaseOf<FRhiCommandContext, TContext>
                         && AltinaEngine::CClassBaseOf<IRhiCmdContextOps, TContext>
        explicit FRhiCmdContextAdapter(TContext& context) : mContext(&context), mOps(&context) {}

        FRhiCmdContextAdapter(FRhiCommandContext& context, IRhiCmdContextOps& ops)
            : mContext(&context), mOps(&ops) {}

        void Begin() override { mContext->Begin(); }
        void End() override { mContext->End(); }

        void RHISetGraphicsPipeline(FRhiPipeline* pipeline) override {
            mOps->RHISetGraphicsPipeline(pipeline);
        }

        void RHISetComputePipeline(FRhiPipeline* pipeline) override {
            mOps->RHISetComputePipeline(pipeline);
        }

        void RHISetPrimitiveTopology(ERhiPrimitiveTopology topology) override {
            mOps->RHISetPrimitiveTopology(topology);
        }

        void RHISetVertexBuffer(u32 slot, const FRhiVertexBufferView& view) override {
            mOps->RHISetVertexBuffer(slot, view);
        }

        void RHISetIndexBuffer(const FRhiIndexBufferView& view) override {
            mOps->RHISetIndexBuffer(view);
        }

        void RHISetViewport(const FRhiViewportRect& viewport) override {
            mOps->RHISetViewport(viewport);
        }

        void RHISetScissor(const FRhiScissorRect& scissor) override {
            mOps->RHISetScissor(scissor);
        }

        void RHISetRenderTargets(u32 colorTargetCount, FRhiTexture* const* colorTargets,
            FRhiTexture* depthTarget) override {
            mOps->RHISetRenderTargets(colorTargetCount, colorTargets, depthTarget);
        }

        void RHIBeginRenderPass(const FRhiRenderPassDesc& desc) override {
            mOps->RHIBeginRenderPass(desc);
        }

        void RHIEndRenderPass() override { mOps->RHIEndRenderPass(); }

        void RHIBeginTransition(const FRhiTransitionCreateInfo& info) override {
            mOps->RHIBeginTransition(info);
        }

        void RHIEndTransition(const FRhiTransitionCreateInfo& info) override {
            mOps->RHIEndTransition(info);
        }

        void RHIClearColor(FRhiTexture* colorTarget, const FRhiClearColor& color) override {
            mOps->RHIClearColor(colorTarget, color);
        }

        void RHISetBindGroup(u32 setIndex, FRhiBindGroup* group, const u32* dynamicOffsets,
            u32 dynamicOffsetCount) override {
            mOps->RHISetBindGroup(setIndex, group, dynamicOffsets, dynamicOffsetCount);
        }

        void RHIDraw(
            u32 vertexCount, u32 instanceCount, u32 firstVertex, u32 firstInstance) override {
            mOps->RHIDraw(vertexCount, instanceCount, firstVertex, firstInstance);
        }

        void RHIDrawIndexed(u32 indexCount, u32 instanceCount, u32 firstIndex, i32 vertexOffset,
            u32 firstInstance) override {
            mOps->RHIDrawIndexed(
                indexCount, instanceCount, firstIndex, vertexOffset, firstInstance);
        }

        void RHIDispatch(u32 groupCountX, u32 groupCountY, u32 groupCountZ) override {
            mOps->RHIDispatch(groupCountX, groupCountY, groupCountZ);
        }

        [[nodiscard]] auto GetRhiContext() const noexcept -> FRhiCommandContext& {
            return *mContext;
        }

    private:
        FRhiCommandContext* mContext = nullptr;
        IRhiCmdContextOps*  mOps     = nullptr;
    };

} // namespace AltinaEngine::Rhi
