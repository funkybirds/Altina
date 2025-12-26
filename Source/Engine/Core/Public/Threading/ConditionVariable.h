// Minimal condition variable wrapper.
#pragma once
#include "../Base/CoreAPI.h"

#include "Common.h"

namespace AltinaEngine::Core::Threading
{

    class FMutex;

    class AE_CORE_API FConditionVariable
    {
    public:
        FConditionVariable() noexcept;
        ~FConditionVariable() noexcept;

        void NotifyOne() noexcept;
        void NotifyAll() noexcept;

        // Waits with the provided mutex locked. Returns true if signaled, false on timeout.
        // Use `kInfiniteWait` for an infinite wait.
        bool Wait(FMutex& Mutex, unsigned long Milliseconds = kInfiniteWait) noexcept;

        // Non-copyable
        FConditionVariable(const FConditionVariable&)            = delete;
        FConditionVariable& operator=(const FConditionVariable&) = delete;

    private:
        void* Impl;
    };

} // namespace AltinaEngine::Core::Threading
