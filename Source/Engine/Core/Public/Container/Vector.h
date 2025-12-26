#ifndef ALTINAENGINE_CORE_PUBLIC_CONTAINER_VECTOR_H
#define ALTINAENGINE_CORE_PUBLIC_CONTAINER_VECTOR_H

#include <initializer_list>
#include <utility>

#include "../Base/CoreAPI.h"
#include "../Types/Aliases.h"
#include "Allocator.h"

namespace AltinaEngine::Core::Container
{

    // Dynamic array similar to std::vector, using the engine allocator.
    template <typename T, typename TAllocatorType = TAllocator<T>> struct TVector
    {
        using value_type      = T;
        using allocator_type  = TAllocatorType;
        using size_type       = usize;
        using pointer         = value_type*;
        using const_pointer   = const value_type*;
        using reference       = value_type&;
        using const_reference = const value_type&;
        using iterator        = value_type*;
        using const_iterator  = const value_type*;

        // ctors / dtors
        constexpr TVector() noexcept : mData(nullptr), mSize(0), mCapacity(0), mAllocator() {}

        explicit TVector(const allocator_type& allocator) noexcept
            : mData(nullptr), mSize(0), mCapacity(0), mAllocator(allocator)
        {
        }

        explicit TVector(
            size_type count, const_reference value = value_type{}, const allocator_type& allocator = allocator_type())
            : mData(nullptr), mSize(0), mCapacity(0), mAllocator(allocator)
        {
            if (count == 0)
            {
                return;
            }

            Reserve(count);
            for (size_type i = 0; i < count; ++i)
            {
                mAllocator.Construct(mData + i, value);
            }
            mSize = count;
        }

        TVector(std::initializer_list<value_type> init, const allocator_type& allocator = allocator_type())
            : mData(nullptr), mSize(0), mCapacity(0), mAllocator(allocator)
        {
            if (init.size() == 0)
            {
                return;
            }

            const size_type count = static_cast<size_type>(init.size());
            Reserve(count);
            size_type index = 0;
            for (const auto& element : init)
            {
                mAllocator.Construct(mData + index, element);
                ++index;
            }
            mSize = count;
        }

        TVector(const TVector& other) : mData(nullptr), mSize(0), mCapacity(0), mAllocator(other.mAllocator)
        {
            if (other.mSize == 0)
            {
                return;
            }

            Reserve(other.mSize);
            for (size_type i = 0; i < other.mSize; ++i)
            {
                mAllocator.Construct(mData + i, other.mData[i]);
            }
            mSize = other.mSize;
        }

        TVector(TVector&& other) noexcept
            : mData(other.mData)
            , mSize(other.mSize)
            , mCapacity(other.mCapacity)
            , mAllocator(AltinaEngine::Move(other.mAllocator))
        {
            other.mData     = nullptr;
            other.mSize     = 0;
            other.mCapacity = 0;
        }

        ~TVector()
        {
            Clear();
            if (mData != nullptr)
            {
                mAllocator.Deallocate(mData, mCapacity);
                mData     = nullptr;
                mSize     = 0;
                mCapacity = 0;
            }
        }

        TVector& operator=(const TVector& other)
        {
            if (this == &other)
            {
                return *this;
            }

            AssignFrom(other);
            return *this;
        }

        TVector& operator=(TVector&& other) noexcept
        {
            if (this == &other)
            {
                return *this;
            }

            Clear();
            if (mData != nullptr)
            {
                mAllocator.Deallocate(mData, mCapacity);
            }

            mData      = other.mData;
            mSize      = other.mSize;
            mCapacity  = other.mCapacity;
            mAllocator = AltinaEngine::Move(other.mAllocator);

            other.mData     = nullptr;
            other.mSize     = 0;
            other.mCapacity = 0;

            return *this;
        }

        // element access
        [[nodiscard]] reference           operator[](size_type index) noexcept { return mData[index]; }

        [[nodiscard]] const_reference     operator[](size_type index) const noexcept { return mData[index]; }

        [[nodiscard]] reference           Front() noexcept { return mData[0]; }

        [[nodiscard]] const_reference     Front() const noexcept { return mData[0]; }

        [[nodiscard]] reference           Back() noexcept { return mData[mSize - 1]; }

        [[nodiscard]] const_reference     Back() const noexcept { return mData[mSize - 1]; }

        [[nodiscard]] pointer             Data() noexcept { return mData; }

        [[nodiscard]] const_pointer       Data() const noexcept { return mData; }

        // iterators (std::vector-compatible names)
        [[nodiscard]] iterator            begin() noexcept { return mData; }

        [[nodiscard]] const_iterator      begin() const noexcept { return mData; }

        [[nodiscard]] const_iterator      cbegin() const noexcept { return mData; }

        [[nodiscard]] iterator            end() noexcept { return mData + mSize; }

        [[nodiscard]] const_iterator      end() const noexcept { return mData + mSize; }

        [[nodiscard]] const_iterator      cend() const noexcept { return mData + mSize; }

        // capacity
        [[nodiscard]] constexpr bool      IsEmpty() const noexcept { return mSize == 0; }

        [[nodiscard]] constexpr size_type Size() const noexcept { return mSize; }

        [[nodiscard]] constexpr size_type Capacity() const noexcept { return mCapacity; }

        void                              Reserve(size_type newCapacity)
        {
            if (newCapacity <= mCapacity)
            {
                return;
            }

            pointer newData = mAllocator.Allocate(static_cast<typename allocator_type::size_type>(newCapacity));

            for (size_type i = 0; i < mSize; ++i)
            {
                mAllocator.Construct(newData + i, AltinaEngine::Move(mData[i]));
                mAllocator.Destroy(mData + i);
            }

            if (mData != nullptr)
            {
                mAllocator.Deallocate(mData, mCapacity);
            }

            mData     = newData;
            mCapacity = newCapacity;
        }

        void Resize(size_type newSize)
        {
            if (newSize < mSize)
            {
                // destroy extra elements
                for (size_type i = newSize; i < mSize; ++i)
                {
                    mAllocator.Destroy(mData + i);
                }
                mSize = newSize;
                return;
            }

            if (newSize > mCapacity)
            {
                Reserve(newSize);
            }

            for (size_type i = mSize; i < newSize; ++i)
            {
                mAllocator.Construct(mData + i);
            }

            mSize = newSize;
        }

        void Clear() noexcept
        {
            for (size_type i = 0; i < mSize; ++i)
            {
                mAllocator.Destroy(mData + i);
            }
            mSize = 0;
        }

        // modifiers
        void PushBack(const_reference value)
        {
            EnsureCapacityForOneMore();
            mAllocator.Construct(mData + mSize, value);
            ++mSize;
        }

        void PushBack(value_type&& value)
        {
            EnsureCapacityForOneMore();
            mAllocator.Construct(mData + mSize, AltinaEngine::Move(value));
            ++mSize;
        }

        template <typename... Args> reference EmplaceBack(Args&&... args)
        {
            EnsureCapacityForOneMore();
            mAllocator.Construct(mData + mSize, std::forward<Args>(args)...);
            ++mSize;
            return mData[mSize - 1];
        }

        void PopBack() noexcept
        {
            if (mSize == 0)
            {
                return;
            }

            --mSize;
            mAllocator.Destroy(mData + mSize);
        }

    private:
        void EnsureCapacityForOneMore()
        {
            if (mSize < mCapacity)
            {
                return;
            }

            const size_type newCapacity = (mCapacity == 0) ? 1 : (mCapacity * 2);
            Reserve(newCapacity);
        }

        void AssignFrom(const TVector& other)
        {
            Clear();

            if (other.mSize > mCapacity)
            {
                if (mData != nullptr)
                {
                    mAllocator.Deallocate(mData, mCapacity);
                }

                mData     = nullptr;
                mCapacity = 0;

                Reserve(other.mSize);
            }

            for (size_type i = 0; i < other.mSize; ++i)
            {
                mAllocator.Construct(mData + i, other.mData[i]);
            }

            mSize = other.mSize;
        }

    private:
        pointer        mData;
        size_type      mSize;
        size_type      mCapacity;
        allocator_type mAllocator;
    };

} // namespace AltinaEngine::Core::Container

#endif // ALTINAENGINE_CORE_PUBLIC_CONTAINER_VECTOR_H
