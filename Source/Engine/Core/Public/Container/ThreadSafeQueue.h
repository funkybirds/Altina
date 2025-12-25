#pragma once

#include "Queue.h"
#include "../Threading/Mutex.h"

namespace AltinaEngine::Core::Container {

template <typename T, typename C = TDeque<T>>
class TThreadSafeQueue
{
public:
    using value_type = T;
    using size_type = usize;

    bool IsEmpty() const noexcept {
        AltinaEngine::Core::Threading::FScopedLock lock(const_cast<AltinaEngine::Core::Threading::FMutex&>(mMutex));
        return mQueue.IsEmpty();
    }

    size_type Size() const noexcept {
        AltinaEngine::Core::Threading::FScopedLock lock(const_cast<AltinaEngine::Core::Threading::FMutex&>(mMutex));
        return mQueue.Size();
    }

    void Push(const value_type& v) {
        AltinaEngine::Core::Threading::FScopedLock lock(mMutex);
        mQueue.Push(v);
    }

    void Push(value_type&& v) {
        AltinaEngine::Core::Threading::FScopedLock lock(mMutex);
        mQueue.Push(std::move(v));
    }

    void Pop() {
        AltinaEngine::Core::Threading::FScopedLock lock(mMutex);
        mQueue.Pop();
    }

    value_type Front() {
        AltinaEngine::Core::Threading::FScopedLock lock(mMutex);
        return mQueue.Front();
    }

    value_type FrontConst() const {
        AltinaEngine::Core::Threading::FScopedLock lock(const_cast<AltinaEngine::Core::Threading::FMutex&>(mMutex));
        return mQueue.Front();
    }

private:
    TQueue<T, C> mQueue;
    mutable AltinaEngine::Core::Threading::FMutex mMutex;
};

} // namespace
