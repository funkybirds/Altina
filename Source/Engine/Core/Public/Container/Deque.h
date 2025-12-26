#pragma once

#include "Allocator.h"
#include "ContainerConfig.h"


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
    template <typename T, typename TAllocator = TAllocator<T>>
    class TDeque
    {
    public:
        using value_type = T;
        using allocator_type = TAllocator;
        using size_type = usize;
        using pointer = value_type*;
        using reference = value_type&;

        /**
         * Forward iterator over the deque. Lightweight and single-pass.
         * This iterator supports increment but is not a full RandomAccessIterator yet.
         */
        struct iterator
        {
            TDeque* parent = nullptr;
            size_type index = 0;

            reference operator*() const { return parent->AtIndex(index); }
            iterator& operator++()
            {
                ++index;
                return *this;
            }
            iterator operator++(int)
            {
                iterator tmp = *this;
                ++*this;
                return tmp;
            }
            bool operator!=(const iterator& o) const { return index != o.index || parent != o.parent; }
            bool operator==(const iterator& o) const { return !(*this != o); }
        };

        /** Default-construct an empty deque. */
        TDeque() noexcept
            : mData(nullptr)
            , mCapacity(0)
            , mSize(0)
            , mHead(0)
            , mAllocator()
        {
        }

        /** Destroy the deque and release storage. */
        ~TDeque()
        {
            Clear();
            if (mData)
            {
                TAllocatorTraits<allocator_type>::Deallocate(mAllocator, mData, mCapacity);
                mData = nullptr;
                mCapacity = 0;
            }
        }

        /** Return true when the container has no elements. */
        bool IsEmpty() const noexcept { return mSize == 0; }
        /** Number of elements currently stored. */
        size_type Size() const noexcept { return mSize; }

        /**
         * Destroy all elements and reset size to zero.
         * Capacity is preserved.
         */
        void Clear() noexcept
        {
            if (mData)
            {
                for (size_type i = 0; i < mSize; ++i)
                {
                    size_type idx = (mHead + i) % mCapacity;
                    TAllocatorTraits<allocator_type>::Destroy(mAllocator, mData + idx);
                }
            }
            mSize = 0;
            mHead = 0;
        }

        /**
         * Push an element to the back. Amortized O(1).
         */
        void PushBack(const value_type& v)
        {
            EnsureCapacityForOneMore();
            size_type idx = (mHead + mSize) % mCapacity;
            TAllocatorTraits<allocator_type>::Construct(mAllocator, mData + idx, v);
            ++mSize;
        }

        /** Move-push an element to the back. */
        void PushBack(value_type&& v)
        {
            EnsureCapacityForOneMore();
            size_type idx = (mHead + mSize) % mCapacity;
            TAllocatorTraits<allocator_type>::Construct(mAllocator, mData + idx, AltinaEngine::Forward<value_type>(v));
            ++mSize;
        }

        /**
         * Push an element to the front. Amortized O(1).
         */
        void PushFront(const value_type& v)
        {
            EnsureCapacityForOneMore();
            mHead = (mHead + mCapacity - 1) % mCapacity;
            TAllocatorTraits<allocator_type>::Construct(mAllocator, mData + mHead, v);
            ++mSize;
        }

        /** Move-push an element to the front. */
        void PushFront(value_type&& v)
        {
            EnsureCapacityForOneMore();
            mHead = (mHead + mCapacity - 1) % mCapacity;
            TAllocatorTraits<allocator_type>::Construct(mAllocator, mData + mHead, AltinaEngine::Forward<value_type>(v));
            ++mSize;
        }

        /** Pop the last element. No-op if empty. */
        void PopBack() noexcept
        {
            if (mSize == 0) return;
            size_type idx = (mHead + mSize - 1) % mCapacity;
            TAllocatorTraits<allocator_type>::Destroy(mAllocator, mData + idx);
            --mSize;
        }

        /** Pop the first element. No-op if empty. */
        void PopFront() noexcept
        {
            if (mSize == 0) return;
            TAllocatorTraits<allocator_type>::Destroy(mAllocator, mData + mHead);
            mHead = (mHead + 1) % mCapacity;
            --mSize;
        }

        /** Access the first element (must be non-empty). */
        reference Front() noexcept { return AtIndex(0); }
        const reference Front() const noexcept { return const_cast<TDeque*>(this)->AtIndex(0); }

        /** Access the last element (must be non-empty). */
        reference Back() noexcept { return AtIndex(mSize - 1); }
        const reference Back() const noexcept { return const_cast<TDeque*>(this)->AtIndex(mSize - 1); }

        /** Iteration from front (inclusive) to back (exclusive). */
        iterator begin() noexcept { return iterator{ this, 0 }; }
        iterator end() noexcept { return iterator{ this, mSize }; }

    private:
        reference AtIndex(size_type i) noexcept
        {
            size_type idx = (mHead + i) % mCapacity;
            return mData[idx];
        }

        const reference AtIndex(size_type i) const noexcept
        {
            return const_cast<TDeque*>(this)->AtIndex(i);
        }

        void EnsureCapacityForOneMore()
        {
            if (mSize < mCapacity && mCapacity > 0) return;
            const size_type newCapacity = (mCapacity == 0) ? 4 : (mCapacity * 2);
            Reallocate(newCapacity);
        }

        void Reallocate(size_type newCapacity)
        {
            pointer newData = TAllocatorTraits<allocator_type>::Allocate(mAllocator, static_cast<typename allocator_type::size_type>(newCapacity));
            // move existing elements
            for (size_type i = 0; i < mSize; ++i)
            {
                size_type oldIdx = (mHead + i) % (mCapacity == 0 ? 1 : mCapacity);
                TAllocatorTraits<allocator_type>::Construct(mAllocator, newData + i, AltinaEngine::Move(*(mData + oldIdx)));
                if (mData)
                    TAllocatorTraits<allocator_type>::Destroy(mAllocator, mData + oldIdx);
            }
            if (mData)
            {
                TAllocatorTraits<allocator_type>::Deallocate(mAllocator, mData, mCapacity);
            }
            mData = newData;
            mCapacity = newCapacity;
            mHead = 0;
        }

    private:
        pointer mData;
        size_type mCapacity;
        size_type mSize;
        size_type mHead;
        allocator_type mAllocator;
    };

} // namespace AltinaEngine::Core::Container
