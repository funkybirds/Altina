// Minimal mutex wrapper avoiding STL, Win32 implementation expected.
#pragma once
#include "../Base/CoreAPI.h"

namespace AltinaEngine::Core::Threading {

    class AE_CORE_API FMutex {
    public:
        FMutex() noexcept;
        ~FMutex() noexcept;

        void               Lock() noexcept;
        auto               TryLock() noexcept -> bool;
        void               Unlock() noexcept;

        [[nodiscard]] auto GetNative() const noexcept -> void*;

        // Non-copyable
        FMutex(const FMutex&)                    = delete;
        auto operator=(const FMutex&) -> FMutex& = delete;

    private:
        void* mImpl; // Opaque pointer to platform implementation
    };

    class AE_CORE_API FScopedLock {
    public:
        explicit FScopedLock(FMutex& InMutex) noexcept;
        ~FScopedLock() noexcept;

        // Non-copyable
        FScopedLock(const FScopedLock&)                    = delete;
        auto operator=(const FScopedLock&) -> FScopedLock& = delete;

    private:
        FMutex& mMutex;
    };

} // namespace AltinaEngine::Core::Threading
