#pragma once

#include "RhiGeneralAPI.h"
#include "Types/Aliases.h"

namespace AltinaEngine::Rhi {
    class AE_RHI_GENERAL_API IRhiCmdContextOps {
    public:
        IRhiCmdContextOps();
        virtual ~IRhiCmdContextOps();

        virtual void RHIDrawIndexed(u32 indexCount, u32 instanceCount, u32 firstIndex,
            i32 vertexOffset, u32 firstInstance) = 0;
        virtual void RHIDispatch(u32 groupCountX, u32 groupCountY, u32 groupCountZ) = 0;
    };

} // namespace AltinaEngine::Rhi
