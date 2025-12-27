#pragma once

#include "Allocator.h"

namespace AltinaEngine::Core::Container
{

    /**
     * TDeque<T, Alloc>
     * A simple double-ended queue implemented as a circular buffer.
     * - Backed by the engine `TAllocator` for allocation.
     * - Amortized O(1) push/pop at both ends.
     * - Iteration order is logical front->back.
     *
     * Usage:
     *   TDeque<int> d;
     *   d.PushBack(1);
     *   d.PushFront(0);
     *   for (auto it = d.begin(); it != d.end(); ++it) { // access element via *it }
     */
    template <typename T, typename TAllocator = TAllocator<T>> class TDeque
    {
    public:
        using TValueType     = T;
        using TAllocatorType = TAllocator;
        using TSizeType      = usize;
        using TPointer       = TValueType*;
        using TReference     = TValueType&;

        struct Iterator
        {
            TDeque*   mParent = nullptr;
            TSizeType mIndex  = 0;

            auto      operator*() const -> TReference { return mParent->AtIndex(mIndex); }
            auto      operator++() -> Iterator&
            {
                ++mIndex;
                return *this;
            }
            auto operator++(int) -> Iterator
            {
                Iterator tmp = *this;
                ++*this;
                return tmp;
            }
            auto operator!=(const Iterator& o) const -> bool { return mIndex != o.mIndex || mParent != o.mParent; }
            auto operator==(const Iterator& o) const -> bool { return !(*this != o); }
        };

        TDeque() noexcept : mData(nullptr), mAllocator() {}

        ~TDeque()
        {
            Clear();
            if (mData)
            {
                TAllocatorTraits<TAllocatorType>::Deallocate(mAllocator, mData, mCapacity);
                mData     = nullptr;
                mCapacity = 0;
            }
        }

        [[nodiscard]] auto IsEmpty() const noexcept -> bool { return mSize == 0; }
        [[nodiscard]] auto Size() const noexcept -> TSizeType { return mSize; }

        void               Clear() noexcept
        {
            if (mData)
            {
                for (TSizeType i = 0; i < mSize; ++i)
                {
                    TSizeType idx = (mHead + i) % mCapacity;
                    TAllocatorTraits<TAllocatorType>::Destroy(mAllocator, mData + idx);
                }
            }
            mSize = 0;
            mHead = 0;
        }

        void PushBack(const TValueType& v)
        {
            EnsureCapacityForOneMore();
            TSizeType idx = (mHead + mSize) % mCapacity;
            TAllocatorTraits<TAllocatorType>::Construct(mAllocator, mData + idx, v);
            ++mSize;
        }

        void PushBack(TValueType&& v)
        {
            EnsureCapacityForOneMore();
            TSizeType idx = (mHead + mSize) % mCapacity;
            TAllocatorTraits<TAllocatorType>::Construct(mAllocator, mData + idx, AltinaEngine::Forward<TValueType>(v));
            ++mSize;
        }

        void PushFront(const TValueType& v)
        {
            EnsureCapacityForOneMore();
            mHead = (mHead + mCapacity - 1) % mCapacity;
            TAllocatorTraits<TAllocatorType>::Construct(mAllocator, mData + mHead, v);
            ++mSize;
        }

        void PushFront(TValueType&& v)
        {
            EnsureCapacityForOneMore();
            mHead = (mHead + mCapacity - 1) % mCapacity;
            TAllocatorTraits<TAllocatorType>::Construct(
                mAllocator, mData + mHead, AltinaEngine::Forward<TValueType>(v));
            ++mSize;
        }

        void PopBack() noexcept
        {
            if (mSize == 0)
                return;
            TSizeType idx = (mHead + mSize - 1) % mCapacity;
            TAllocatorTraits<TAllocatorType>::Destroy(mAllocator, mData + idx);
            --mSize;
        }

        void PopFront() noexcept
        {
            if (mSize == 0)
                return;
            TAllocatorTraits<TAllocatorType>::Destroy(mAllocator, mData + mHead);
            mHead = (mHead + 1) % mCapacity;
            --mSize;
        }

        auto Front() noexcept -> TReference { return AtIndex(0); }
        auto Front() const noexcept -> TReference { return const_cast<TDeque*>(this)->AtIndex(0); }

        auto Back() noexcept -> TReference { return AtIndex(mSize - 1); }
        auto Back() const noexcept -> TReference { return const_cast<TDeque*>(this)->AtIndex(mSize - 1); }

        auto begin() noexcept -> Iterator { return Iterator{ this, 0 }; }   // NOLINT(*-identifier-naming)
        auto end() noexcept -> Iterator { return Iterator{ this, mSize }; } // NOLINT(*-identifier-naming)

    private:
        auto AtIndex(TSizeType i) noexcept -> TReference
        {
            TSizeType idx = (mHead + i) % mCapacity;
            return mData[idx];
        }

        auto AtIndex(TSizeType i) const noexcept -> TReference { return const_cast<TDeque*>(this)->AtIndex(i); }

        void EnsureCapacityForOneMore()
        {
            if (mSize < mCapacity && mCapacity > 0)
                return;
            const TSizeType newCapacity = (mCapacity == 0) ? 4 : (mCapacity * 2);
            Reallocate(newCapacity);
        }

        void Reallocate(TSizeType newCapacity)
        {
            TPointer newData = TAllocatorTraits<TAllocatorType>::Allocate(
                mAllocator, static_cast<TAllocatorType::TSizeType>(newCapacity));
            // move existing elements
            for (TSizeType i = 0; i < mSize; ++i)
            {
                TSizeType oldIdx = (mHead + i) % (mCapacity == 0 ? 1 : mCapacity);
                TAllocatorTraits<TAllocatorType>::Construct(
                    mAllocator, newData + i, AltinaEngine::Move(*(mData + oldIdx)));
                if (mData)
                    TAllocatorTraits<TAllocatorType>::Destroy(mAllocator, mData + oldIdx);
            }
            if (mData)
            {
                TAllocatorTraits<TAllocatorType>::Deallocate(mAllocator, mData, mCapacity);
            }
            mData     = newData;
            mCapacity = newCapacity;
            mHead     = 0;
        }

        TPointer       mData;
        TSizeType      mCapacity{ 0 };
        TSizeType      mSize{ 0 };
        TSizeType      mHead{ 0 };
        TAllocatorType mAllocator;
    };

} // namespace AltinaEngine::Core::Container
