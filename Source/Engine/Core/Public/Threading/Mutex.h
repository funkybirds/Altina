// Minimal mutex wrapper avoiding STL, Win32 implementation expected.
#pragma once
#include "../Base/CoreAPI.h"

namespace AltinaEngine::Core::Threading
{

    class AE_CORE_API FMutex
    {
    public:
        FMutex() noexcept;
        ~FMutex() noexcept;

        void  Lock() noexcept;
        bool  TryLock() noexcept;
        void  Unlock() noexcept;

        void* GetNative() const noexcept;

        // Non-copyable
        FMutex(const FMutex&)            = delete;
        FMutex& operator=(const FMutex&) = delete;

    private:
        void* Impl; // Opaque pointer to platform implementation
    };

    class AE_CORE_API FScopedLock
    {
    public:
        explicit FScopedLock(FMutex& InMutex) noexcept;
        ~FScopedLock() noexcept;

        // Non-copyable
        FScopedLock(const FScopedLock&)            = delete;
        FScopedLock& operator=(const FScopedLock&) = delete;

    private:
        FMutex& Mutex;
    };

} // namespace AltinaEngine::Core::Threading
