
// Minimal event wrapper (manual-reset/auto-reset) using Win32 Event.
#pragma once
#include "../Base/CoreAPI.h"

namespace AltinaEngine::Core::Threading {

enum class EEventResetMode { Auto, Manual };

// Use a portable infinite-wait sentinel instead of exposing platform headers.
inline constexpr unsigned long kInfiniteWait = static_cast<unsigned long>(-1);

class AE_CORE_API FEvent {
public:
    FEvent(bool bInitiallySignaled = false, EEventResetMode ResetMode = EEventResetMode::Auto) noexcept;
    ~FEvent() noexcept;

    void Set() noexcept;
    void Reset() noexcept;
    bool Wait(unsigned long Milliseconds = kInfiniteWait) noexcept;

    // Non-copyable
    FEvent(const FEvent&) = delete;
    FEvent& operator=(const FEvent&) = delete;
private:
    void* Impl;
};

} // namespace
