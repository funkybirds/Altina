#pragma once

#include "Stack.h"
#include "../Threading/Mutex.h"

namespace AltinaEngine::Core::Container {

template <typename T, typename C = TDeque<T>>
class TThreadSafeStack
{
public:
    using value_type = T;
    using size_type = usize;

    bool IsEmpty() const noexcept {
        AltinaEngine::Core::Threading::FScopedLock lock(const_cast<AltinaEngine::Core::Threading::FMutex&>(mMutex));
        return mStack.IsEmpty();
    }

    size_type Size() const noexcept {
        AltinaEngine::Core::Threading::FScopedLock lock(const_cast<AltinaEngine::Core::Threading::FMutex&>(mMutex));
        return mStack.Size();
    }

    void Push(const value_type& v) {
        AltinaEngine::Core::Threading::FScopedLock lock(mMutex);
        mStack.Push(v);
    }

    void Push(value_type&& v) {
        AltinaEngine::Core::Threading::FScopedLock lock(mMutex);
        mStack.Push(std::move(v));
    }

    void Pop() {
        AltinaEngine::Core::Threading::FScopedLock lock(mMutex);
        mStack.Pop();
    }

    value_type Top() {
        AltinaEngine::Core::Threading::FScopedLock lock(mMutex);
        return mStack.Top();
    }

    value_type TopConst() const {
        AltinaEngine::Core::Threading::FScopedLock lock(const_cast<AltinaEngine::Core::Threading::FMutex&>(mMutex));
        return mStack.Top();
    }

private:
    TStack<T, C> mStack;
    mutable AltinaEngine::Core::Threading::FMutex mMutex;
};

} // namespace
