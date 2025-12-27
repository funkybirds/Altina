#pragma once

#include "Stack.h"
#include "../Threading/Mutex.h"

namespace AltinaEngine::Core::Container
{

    template <typename T, typename C = TDeque<T>> class TThreadSafeStack
    {
    public:
        using TValueType = T;
        using TSizeType  = usize;

        auto IsEmpty() const noexcept -> bool
        {
            Threading::FScopedLock lock(const_cast<Threading::FMutex&>(mMutex));
            return mStack.IsEmpty();
        }

        auto Size() const noexcept -> TSizeType
        {
            Threading::FScopedLock lock(const_cast<Threading::FMutex&>(mMutex));
            return mStack.Size();
        }

        void Push(const TValueType& v)
        {
            Threading::FScopedLock lock(mMutex);
            mStack.Push(v);
        }

        void Push(TValueType&& v)
        {
            Threading::FScopedLock lock(mMutex);
            mStack.Push(AltinaEngine::Move(v));
        }

        void Pop()
        {
            Threading::FScopedLock lock(mMutex);
            mStack.Pop();
        }

        auto Top() -> TValueType
        {
            Threading::FScopedLock lock(mMutex);
            return mStack.Top();
        }

        auto TopConst() const -> TValueType
        {
            Threading::FScopedLock lock(const_cast<Threading::FMutex&>(mMutex));
            return mStack.Top();
        }

    private:
        TStack<T, C>              mStack;
        mutable Threading::FMutex mMutex;
    };

} // namespace AltinaEngine::Core::Container
