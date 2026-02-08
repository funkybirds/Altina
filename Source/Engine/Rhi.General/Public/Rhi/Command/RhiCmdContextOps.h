#pragma once

#include "RhiGeneralAPI.h"
#include "Rhi/RhiFwd.h"
#include "Types/Aliases.h"

namespace AltinaEngine::Rhi {
    class AE_RHI_GENERAL_API IRhiCmdContextOps {
    public:
        IRhiCmdContextOps();
        virtual ~IRhiCmdContextOps();

        virtual void RHISetGraphicsPipeline(FRhiPipeline* pipeline) = 0;
        virtual void RHISetComputePipeline(FRhiPipeline* pipeline) = 0;
        virtual void RHISetRenderTargets(u32 colorTargetCount,
            FRhiTexture* const* colorTargets, FRhiTexture* depthTarget) = 0;
        virtual void RHISetBindGroup(u32 setIndex, FRhiBindGroup* group,
            const u32* dynamicOffsets = nullptr, u32 dynamicOffsetCount = 0U) = 0;
        virtual void RHIDrawIndexed(u32 indexCount, u32 instanceCount, u32 firstIndex,
            i32 vertexOffset, u32 firstInstance) = 0;
        virtual void RHIDispatch(u32 groupCountX, u32 groupCountY, u32 groupCountZ) = 0;
    };

} // namespace AltinaEngine::Rhi
