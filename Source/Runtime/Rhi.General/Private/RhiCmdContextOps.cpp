#include "Rhi/Command/RhiCmdContextOps.h"
#include "Rhi/RhiStructs.h"

namespace AltinaEngine::Rhi {
    IRhiCmdContextOps::IRhiCmdContextOps()  = default;
    IRhiCmdContextOps::~IRhiCmdContextOps() = default;

    void IRhiCmdContextOps::RHISetVertexBuffers(
        u32 firstSlot, const FRhiVertexBufferView* views, u32 viewCount) {
        if (views == nullptr || viewCount == 0U) {
            return;
        }

        for (u32 index = 0U; index < viewCount; ++index) {
            RHISetVertexBuffer(firstSlot + index, views[index]);
        }
    }
} // namespace AltinaEngine::Rhi
