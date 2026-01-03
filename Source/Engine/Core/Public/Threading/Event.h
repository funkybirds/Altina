
// Minimal event wrapper (manual-reset/auto-reset) using Win32 Event.
#pragma once
#include "Types/Traits.h"

#include "Common.h"

namespace AltinaEngine::Core::Threading {

    enum class EEventResetMode : u8 {
        Auto,
        Manual
    };

    class AE_CORE_API FEvent {
    public:
        FEvent(bool         bInitiallySignaled = false,
            EEventResetMode ResetMode          = EEventResetMode::Auto) noexcept;
        ~FEvent() noexcept;

        void Set() noexcept;
        void Reset() noexcept;
        auto Wait(unsigned long Milliseconds = kInfiniteWait) noexcept -> bool;

        // Non-copyable
        FEvent(const FEvent&)                    = delete;
        auto operator=(const FEvent&) -> FEvent& = delete;

    private:
        void* mImpl;
    };

} // namespace AltinaEngine::Core::Threading
