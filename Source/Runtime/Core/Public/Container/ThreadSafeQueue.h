#pragma once

#include "Queue.h"
#include "../Threading/Mutex.h"

using AltinaEngine::Move;
namespace AltinaEngine::Core::Container {

    template <typename T, typename C = TDeque<T>> class TThreadSafeQueue {
    public:
        using TValueType = T;
        using TSizeType  = usize;

        auto IsEmpty() const noexcept -> bool {
            Threading::FScopedLock lock(const_cast<Threading::FMutex&>(mMutex));
            return mQueue.IsEmpty();
        }

        auto Size() const noexcept -> TSizeType {
            Threading::FScopedLock lock(const_cast<Threading::FMutex&>(mMutex));
            return mQueue.Size();
        }

        void Push(const TValueType& v) {
            Threading::FScopedLock lock(mMutex);
            mQueue.Push(v);
        }

        void Push(TValueType&& v) {
            Threading::FScopedLock lock(mMutex);
            mQueue.Push(Move(v));
        }

        void Pop() {
            Threading::FScopedLock lock(mMutex);
            mQueue.Pop();
        }

        auto Front() -> TValueType {
            Threading::FScopedLock lock(mMutex);
            return mQueue.Front();
        }

        auto FrontConst() const -> TValueType {
            Threading::FScopedLock lock(const_cast<Threading::FMutex&>(mMutex));
            return mQueue.Front();
        }

    private:
        TQueue<T, C>              mQueue;
        mutable Threading::FMutex mMutex;
    };

} // namespace AltinaEngine::Core::Container
