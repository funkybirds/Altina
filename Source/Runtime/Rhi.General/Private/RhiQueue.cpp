#include "Rhi/RhiQueue.h"

namespace AltinaEngine::Rhi {

    FRhiQueue::FRhiQueue(ERhiQueueType type, FRhiResourceDeleteQueue* deleteQueue)
        : FRhiResource(deleteQueue), mType(type) {}

    auto FRhiQueue::GetType() const noexcept -> ERhiQueueType { return mType; }

} // namespace AltinaEngine::Rhi
