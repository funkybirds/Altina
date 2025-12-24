#pragma once

#include <mutex>

namespace AltinaEngine::Core::Container
{

    // Lightweight RAII scoped lock using std::mutex (STL allowed for concurrency)
    class FScopedLock
    {
    public:
        explicit FScopedLock(std::mutex& m) noexcept
            : mLock(m)
        {
        }

        ~FScopedLock() = default;

        // non-copyable
        FScopedLock(const FScopedLock&) = delete;
        FScopedLock& operator=(const FScopedLock&) = delete;

    private:
        std::unique_lock<std::mutex> mLock;
    };

} // namespace AltinaEngine::Core::Container
