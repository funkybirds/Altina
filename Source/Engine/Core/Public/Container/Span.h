#pragma once

#include "Types/Aliases.h"
#include "Types/Traits.h"
#include "Math/NumericConstants.h"
#include "Array.h"
#include "Vector.h"

namespace AltinaEngine::Core::Container {
    using Math::FNumericConstants;

    // Does not expose any constructor that directly takes a raw pointer.
    template <typename T, usize Extent = FNumericConstants::kDynamicSized> class TSpan {
    public:
        using TElementType    = T;
        using TValueType      = TRemoveCV<T>::TType;
        using TSizeType       = usize;
        using TDifferenceType = isize;
        using TPointer        = TElementType*;
        using TConstPointer   = const TElementType*;
        using TReference      = TElementType&;
        using TConstReference = const TElementType&;
        using TIterator       = TPointer;
        using TConstIterator  = TConstPointer;

        static constexpr TSizeType kExtent = Extent;

        // Default constructor only for dynamic extent
        constexpr TSpan() noexcept
            requires(Extent == FNumericConstants::kDynamicSized)
            : mData(nullptr), mSize(0) {}

        // From C-style array (non-const)
        template <usize N>
        constexpr TSpan(TElementType (&array)[N]) noexcept : mData(array), mSize(N) {
            static_assert(Extent == FNumericConstants::kDynamicSized || Extent == N,
                "TSpan static extent must match source size");
        }

        // From C-style array (const)
        template <usize N>
        constexpr TSpan(const TElementType (&array)[N]) noexcept
            : mData(const_cast<TElementType*>(array)), mSize(N) {
            static_assert(Extent == FNumericConstants::kDynamicSized || Extent == N,
                "TSpan static extent must match source size");
        }

        // From fixed-size engine array
        template <usize N>
        constexpr TSpan(TArray<TElementType, N>& array) noexcept
            : mData(array.Data()), mSize(TArray<TElementType, N>::Size()) {
            static_assert(Extent == FNumericConstants::kDynamicSized || Extent == N,
                "TSpan static extent must match source size");
        }

        template <usize N>
        constexpr TSpan(const TArray<TElementType, N>& array) noexcept
            : mData(const_cast<TElementType*>(array.Data()))
            , mSize(TArray<TElementType, N>::Size()) {
            static_assert(Extent == FNumericConstants::kDynamicSized || Extent == N,
                "TSpan static extent must match source size");
        }

        // From dynamic engine vector (dynamic extent only)
        template <typename TAllocatorType>
        constexpr TSpan(TVector<TElementType, TAllocatorType>& vector) noexcept
            : mData(vector.IsEmpty() ? nullptr : &vector[0]), mSize(vector.Size()) {
            static_assert(Extent == FNumericConstants::kDynamicSized,
                "TSpan over TVector must use dynamic extent");
        }

        template <typename TAllocatorType>
        constexpr TSpan(const TVector<TElementType, TAllocatorType>& vector) noexcept
            : mData(vector.IsEmpty() ? nullptr : const_cast<TElementType*>(&vector[0]))
            , mSize(vector.Size()) {
            static_assert(Extent == FNumericConstants::kDynamicSized,
                "TSpan over TVector must use dynamic extent");
        }

        // Observers
        [[nodiscard]] constexpr auto Size() const noexcept -> TSizeType { return mSize; }

        [[nodiscard]] constexpr auto IsEmpty() const noexcept -> bool { return mSize == 0; }

        [[nodiscard]] constexpr auto ExtentValue() const noexcept -> TSizeType {
            return (Extent == FNumericConstants::kDynamicSized) ? mSize : Extent;
        }

        // Element access
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

        // Iteration
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

    private:
        TPointer  mData;
        TSizeType mSize;
    };

} // namespace AltinaEngine::Core::Container
