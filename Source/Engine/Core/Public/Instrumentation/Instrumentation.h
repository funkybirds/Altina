#pragma once

#include "../Base/CoreAPI.h"
#include "../Types/Aliases.h"
#include "../Container/String.h"

namespace AltinaEngine::Core::Instrumentation {
    using Container::FString;

    // Set the name for the current thread. Name pointer is copied into engine string storage.
    AE_CORE_API void        SetCurrentThreadName(const char* name) noexcept;

    // Get the name previously set for this thread. Returns empty string view if none set.
    AE_CORE_API const char* GetCurrentThreadName() noexcept;

    // Simple global counters. Counter names are engine strings; deltas can be negative.
    AE_CORE_API void        IncrementCounter(const char* name, long long delta = 1) noexcept;
    AE_CORE_API long long   GetCounterValue(const char* name) noexcept;

    // Timing aggregates recorded via scoped timers. Value recorded in milliseconds.
    AE_CORE_API void        RecordTimingMs(const char* name, unsigned long long ms) noexcept;
    AE_CORE_API void        GetTimingAggregate(
               const char* name, unsigned long long& outTotalMs, unsigned long long& outCount) noexcept;

    // Lightweight RAII timer that records the elapsed time (ms) to the named aggregate on
    // destruction.
    class AE_CORE_API FScopedTimer {
    public:
        explicit FScopedTimer(const char* name) noexcept;
        ~FScopedTimer() noexcept;

        FScopedTimer(const FScopedTimer&)                    = delete;
        auto operator=(const FScopedTimer&) -> FScopedTimer& = delete;

    private:
        const char*        mName;
        unsigned long long mStartMs;
    };

} // namespace AltinaEngine::Core::Instrumentation
