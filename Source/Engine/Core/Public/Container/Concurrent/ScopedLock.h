#pragma once

#include "../../Threading/Mutex.h"

namespace AltinaEngine::Core::Container
{

    // Lightweight RAII scoped lock using engine FMutex
    class FScopedLock
    {
    public:
        explicit FScopedLock(AltinaEngine::Core::Threading::FMutex& m) noexcept
            : mLock(m)
        {
        }

        ~FScopedLock() = default;

        // non-copyable
        FScopedLock(const FScopedLock&) = delete;
        FScopedLock& operator=(const FScopedLock&) = delete;

    private:
        AltinaEngine::Core::Threading::FScopedLock mLock;
    };

} // namespace AltinaEngine::Core::Container
