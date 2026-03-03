#include "Rhi/RhiTransition.h"

namespace AltinaEngine::Rhi {
    FRhiTransition::FRhiTransition(
        const FRhiTransitionDesc& desc, FRhiResourceDeleteQueue* deleteQueue) noexcept
        : FRhiResource(deleteQueue), mDesc(desc) {}

    FRhiTransition::~FRhiTransition() = default;
} // namespace AltinaEngine::Rhi
