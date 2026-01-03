#pragma once

#include "../../Threading/Mutex.h"

namespace AltinaEngine::Core::Container {

    // Lightweight RAII scoped lock using engine FMutex
    class FScopedLock {
    public:
        explicit FScopedLock(Threading::FMutex& m) noexcept : mLock(m) {}

        ~FScopedLock() = default;

        // non-copyable
        FScopedLock(const FScopedLock&)                    = delete;
        auto operator=(const FScopedLock&) -> FScopedLock& = delete;

        // non-movable
        FScopedLock(FScopedLock&&)                    = delete;
        auto operator=(FScopedLock&&) -> FScopedLock& = delete;

    private:
        Threading::FScopedLock mLock;
    };

} // namespace AltinaEngine::Core::Container
