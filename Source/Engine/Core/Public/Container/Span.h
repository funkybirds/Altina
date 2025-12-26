#pragma once

#include "../Types/Aliases.h"
#include "../Types/Traits.h"
#include "../Math/NumericConstants.h"
#include "Array.h"
#include "Vector.h"

namespace AltinaEngine::Core::Container
{
    using AltinaEngine::Core::Math::FNumericConstants;

    // Does not expose any constructor that directly takes a raw pointer.
    template <typename T, usize Extent = FNumericConstants::kDynamicSized> class TSpan
    {
    public:
        using element_type    = T;
        using value_type      = typename TRemoveCV<T>::Type;
        using size_type       = usize;
        using difference_type = isize;
        using pointer         = element_type*;
        using const_pointer   = const element_type*;
        using reference       = element_type&;
        using const_reference = const element_type&;
        using iterator        = pointer;
        using const_iterator  = const_pointer;

        static constexpr size_type kExtent = Extent;

        // Default constructor only for dynamic extent
        constexpr TSpan() noexcept
            requires(Extent == FNumericConstants::kDynamicSized)
            : mData(nullptr), mSize(0)
        {
        }

        // From C-style array (non-const)
        template <usize N> constexpr TSpan(element_type (&array)[N]) noexcept : mData(array), mSize(N)
        {
            static_assert(Extent == FNumericConstants::kDynamicSized || Extent == N,
                "TSpan static extent must match source size");
        }

        // From C-style array (const)
        template <usize N>
        constexpr TSpan(const element_type (&array)[N]) noexcept : mData(const_cast<element_type*>(array)), mSize(N)
        {
            static_assert(Extent == FNumericConstants::kDynamicSized || Extent == N,
                "TSpan static extent must match source size");
        }

        // From fixed-size engine array
        template <usize N>
        constexpr TSpan(TArray<element_type, N>& array) noexcept
            : mData(array.Data()), mSize(TArray<element_type, N>::Size())
        {
            static_assert(Extent == FNumericConstants::kDynamicSized || Extent == N,
                "TSpan static extent must match source size");
        }

        template <usize N>
        constexpr TSpan(const TArray<element_type, N>& array) noexcept
            : mData(const_cast<element_type*>(array.Data())), mSize(TArray<element_type, N>::Size())
        {
            static_assert(Extent == FNumericConstants::kDynamicSized || Extent == N,
                "TSpan static extent must match source size");
        }

        // From dynamic engine vector (dynamic extent only)
        template <typename TAllocatorType>
        constexpr TSpan(TVector<element_type, TAllocatorType>& vector) noexcept
            : mData(vector.IsEmpty() ? nullptr : &vector[0]), mSize(vector.Size())
        {
            static_assert(Extent == FNumericConstants::kDynamicSized, "TSpan over TVector must use dynamic extent");
        }

        template <typename TAllocatorType>
        constexpr TSpan(const TVector<element_type, TAllocatorType>& vector) noexcept
            : mData(vector.IsEmpty() ? nullptr : const_cast<element_type*>(&vector[0])), mSize(vector.Size())
        {
            static_assert(Extent == FNumericConstants::kDynamicSized, "TSpan over TVector must use dynamic extent");
        }

        // Observers
        [[nodiscard]] constexpr size_type Size() const noexcept { return mSize; }

        [[nodiscard]] constexpr bool      IsEmpty() const noexcept { return mSize == 0; }

        [[nodiscard]] constexpr size_type ExtentValue() const noexcept
        {
            return (Extent == FNumericConstants::kDynamicSized) ? mSize : Extent;
        }

        // Element access
        [[nodiscard]] reference       operator[](size_type index) noexcept { return mData[index]; }

        [[nodiscard]] const_reference operator[](size_type index) const noexcept { return mData[index]; }

        [[nodiscard]] reference       Front() noexcept { return mData[0]; }

        [[nodiscard]] const_reference Front() const noexcept { return mData[0]; }

        [[nodiscard]] reference       Back() noexcept { return mData[mSize - 1]; }

        [[nodiscard]] const_reference Back() const noexcept { return mData[mSize - 1]; }

        [[nodiscard]] pointer         Data() noexcept { return mData; }

        [[nodiscard]] const_pointer   Data() const noexcept { return mData; }

        // Iteration
        [[nodiscard]] iterator        begin() noexcept { return mData; }

        [[nodiscard]] const_iterator  begin() const noexcept { return mData; }

        [[nodiscard]] const_iterator  cbegin() const noexcept { return mData; }

        [[nodiscard]] iterator        end() noexcept { return mData + mSize; }

        [[nodiscard]] const_iterator  end() const noexcept { return mData + mSize; }

        [[nodiscard]] const_iterator  cend() const noexcept { return mData + mSize; }

    private:
        pointer   mData;
        size_type mSize;
    };

} // namespace AltinaEngine::Core::Container
