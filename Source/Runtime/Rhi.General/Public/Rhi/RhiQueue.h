#pragma once

#include "RhiGeneralAPI.h"
#include "Rhi/RhiEnums.h"
#include "Rhi/RhiResource.h"
#include "Types/Aliases.h"

namespace AltinaEngine::Rhi {
    struct FRhiSubmitInfo;
    struct FRhiPresentInfo;
    class FRhiFence;

    class AE_RHI_GENERAL_API FRhiQueue : public FRhiResource {
    public:
        FRhiQueue(ERhiQueueType type, FRhiResourceDeleteQueue* deleteQueue = nullptr);
        ~FRhiQueue() override = default;

        FRhiQueue(const FRhiQueue&)                                  = delete;
        FRhiQueue(FRhiQueue&&)                                       = delete;
        auto               operator=(const FRhiQueue&) -> FRhiQueue& = delete;
        auto               operator=(FRhiQueue&&) -> FRhiQueue&      = delete;

        [[nodiscard]] auto GetType() const noexcept -> ERhiQueueType;

        virtual void       Submit(const FRhiSubmitInfo& info)   = 0;
        virtual void       Signal(FRhiFence* fence, u64 value)  = 0;
        virtual void       Wait(FRhiFence* fence, u64 value)    = 0;
        virtual void       WaitIdle()                           = 0;
        virtual void       Present(const FRhiPresentInfo& info) = 0;

    private:
        ERhiQueueType mType = ERhiQueueType::Graphics;
    };

} // namespace AltinaEngine::Rhi
