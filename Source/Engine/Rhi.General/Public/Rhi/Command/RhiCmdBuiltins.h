#pragma once

#include "RhiGeneralAPI.h"
#include "Rhi/Command/RhiCmd.h"
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
