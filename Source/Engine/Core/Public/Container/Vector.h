#ifndef ALTINAENGINE_CORE_PUBLIC_CONTAINER_VECTOR_H
#define ALTINAENGINE_CORE_PUBLIC_CONTAINER_VECTOR_H

#include <initializer_list>
#include <utility>

#include "../Base/CoreAPI.h"
#include "../Types/Aliases.h"
#include "Allocator.h"

namespace AltinaEngine::Core::Container {

    // Dynamic array similar to std::vector, using the engine allocator.
    template <typename T, typename TAllocatorType = TAllocator<T>> struct TVector {
        using TValueType      = T;
        using TSizeType       = usize;
        using TPointer        = TValueType*;
        using TConstPointer   = const TValueType*;
        using TReference      = TValueType&;
        using TConstReference = const TValueType&;
        using TIterator       = TValueType*;
        using TConstIterator  = const TValueType*;

        // ctors / dtors
        constexpr TVector() noexcept : mData(nullptr), mSize(0), mCapacity(0), mAllocator() {}

        explicit TVector(const TAllocatorType& allocator) noexcept
            : mData(nullptr), mSize(0), mCapacity(0), mAllocator(allocator) {}

        explicit TVector(TSizeType count, TConstReference value = TValueType{},
            const TAllocatorType& allocator = TAllocatorType())
            : mData(nullptr), mSize(0), mCapacity(0), mAllocator(allocator) {
            if (count == 0) {
                return;
            }

            Reserve(count);
            for (TSizeType i = 0; i < count; ++i) {
                mAllocator.Construct(mData + i, value);
            }
            mSize = count;
        }

        TVector(std::initializer_list<TValueType> init,
            const TAllocatorType&                 allocator = TAllocatorType())
            : mData(nullptr), mSize(0), mCapacity(0), mAllocator(allocator) {
            if (init.size() == 0) {
                return;
            }

            const auto count = static_cast<TSizeType>(init.size());
            Reserve(count);
            TSizeType index = 0;
            for (const auto& element : init) {
                mAllocator.Construct(mData + index, element);
                ++index;
            }
            mSize = count;
        }

        TVector(const TVector& other)
            : mData(nullptr), mSize(0), mCapacity(0), mAllocator(other.mAllocator) {
            if (other.mSize == 0) {
                return;
            }

            Reserve(other.mSize);
            for (TSizeType i = 0; i < other.mSize; ++i) {
                mAllocator.Construct(mData + i, other.mData[i]);
            }
            mSize = other.mSize;
        }

        TVector(TVector&& other) noexcept
            : mData(other.mData)
            , mSize(other.mSize)
            , mCapacity(other.mCapacity)
            , mAllocator(AltinaEngine::Move(other.mAllocator)) {
            other.mData     = nullptr;
            other.mSize     = 0;
            other.mCapacity = 0;
        }

        ~TVector() {
            Clear();
            if (mData != nullptr) {
                mAllocator.Deallocate(mData, mCapacity);
                mData     = nullptr;
                mSize     = 0;
                mCapacity = 0;
            }
        }

        TVector& operator=(const TVector& other) {
            if (this == &other) {
                return *this;
            }

            AssignFrom(other);
            return *this;
        }

        TVector& operator=(TVector&& other) noexcept {
            if (this == &other) {
                return *this;
            }

            Clear();
            if (mData != nullptr) {
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
        [[nodiscard]] auto operator[](TSizeType index) noexcept -> TReference {
            return mData[index];
        }

        [[nodiscard]] auto operator[](TSizeType index) const noexcept -> TConstReference {
            return mData[index];
        }

        [[nodiscard]] auto Front() noexcept -> TReference { return mData[0]; }

        [[nodiscard]] auto Front() const noexcept -> TConstReference { return mData[0]; }

        [[nodiscard]] auto Back() noexcept -> TReference { return mData[mSize - 1]; }

        [[nodiscard]] auto Back() const noexcept -> TConstReference { return mData[mSize - 1]; }

        [[nodiscard]] auto Data() noexcept -> TPointer { return mData; }

        [[nodiscard]] auto Data() const noexcept -> TConstPointer { return mData; }

        // iterators (std::vector-compatible names)
        [[nodiscard]] auto begin() noexcept -> TIterator {
            return mData;
        } // NOLINT(*-identifier-naming)

        [[nodiscard]] auto begin() const noexcept -> TConstIterator {
            return mData;
        } // NOLINT(*-identifier-naming)

        [[nodiscard]] auto cbegin() const noexcept -> TConstIterator {
            return mData;
        } // NOLINT(*-identifier-naming)

        [[nodiscard]] auto end() noexcept -> TIterator {
            return mData + mSize;
        } // NOLINT(*-identifier-naming)

        [[nodiscard]] auto end() const noexcept -> TConstIterator {
            return mData + mSize;
        } // NOLINT(*-identifier-naming)

        [[nodiscard]] auto cend() const noexcept -> TConstIterator {
            return mData + mSize;
        } // NOLINT(*-identifier-naming)

        // capacity
        [[nodiscard]] constexpr auto IsEmpty() const noexcept -> bool { return mSize == 0; }

        [[nodiscard]] constexpr auto Size() const noexcept -> TSizeType { return mSize; }

        [[nodiscard]] constexpr auto Capacity() const noexcept -> TSizeType { return mCapacity; }

        void                         Reserve(TSizeType newCapacity) {
            if (newCapacity <= mCapacity) {
                return;
            }

            TPointer newData =
                mAllocator.Allocate(static_cast<typename TAllocatorType::TSizeType>(newCapacity));

            for (TSizeType i = 0; i < mSize; ++i) {
                mAllocator.Construct(newData + i, AltinaEngine::Move(mData[i]));
                mAllocator.Destroy(mData + i);
            }

            if (mData != nullptr) {
                mAllocator.Deallocate(mData, mCapacity);
            }

            mData     = newData;
            mCapacity = newCapacity;
        }

        void Resize(TSizeType newSize) {
            if (newSize < mSize) {
                // destroy extra elements
                for (TSizeType i = newSize; i < mSize; ++i) {
                    mAllocator.Destroy(mData + i);
                }
                mSize = newSize;
                return;
            }

            if (newSize > mCapacity) {
                Reserve(newSize);
            }

            for (TSizeType i = mSize; i < newSize; ++i) {
                mAllocator.Construct(mData + i);
            }

            mSize = newSize;
        }

        void Clear() noexcept {
            for (TSizeType i = 0; i < mSize; ++i) {
                mAllocator.Destroy(mData + i);
            }
            mSize = 0;
        }

        // modifiers
        void PushBack(TConstReference value) {
            EnsureCapacityForOneMore();
            mAllocator.Construct(mData + mSize, value);
            ++mSize;
        }

        void PushBack(TValueType&& value) {
            EnsureCapacityForOneMore();
            mAllocator.Construct(mData + mSize, AltinaEngine::Move(value));
            ++mSize;
        }

        template <typename... Args> auto EmplaceBack(Args&&... args) -> TReference {
            EnsureCapacityForOneMore();
            mAllocator.Construct(mData + mSize, std::forward<Args>(args)...);
            ++mSize;
            return mData[mSize - 1];
        }

        void PopBack() noexcept {
            if (mSize == 0) {
                return;
            }

            --mSize;
            mAllocator.Destroy(mData + mSize);
        }

    private:
        void EnsureCapacityForOneMore() {
            if (mSize < mCapacity) {
                return;
            }

            const TSizeType newCapacity = (mCapacity == 0) ? 1 : (mCapacity * 2);
            Reserve(newCapacity);
        }

        void AssignFrom(const TVector& other) {
            Clear();

            if (other.mSize > mCapacity) {
                if (mData != nullptr) {
                    mAllocator.Deallocate(mData, mCapacity);
                }

                mData     = nullptr;
                mCapacity = 0;

                Reserve(other.mSize);
            }

            for (TSizeType i = 0; i < other.mSize; ++i) {
                mAllocator.Construct(mData + i, other.mData[i]);
            }

            mSize = other.mSize;
        }

    private:
        TPointer       mData;
        TSizeType      mSize;
        TSizeType      mCapacity;
        TAllocatorType mAllocator;
    };

} // namespace AltinaEngine::Core::Container

#endif // ALTINAENGINE_CORE_PUBLIC_CONTAINER_VECTOR_H
