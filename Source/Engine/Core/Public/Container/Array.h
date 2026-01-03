#ifndef ALTINAENGINE_CORE_PUBLIC_CONTAINER_ARRAY_H
#define ALTINAENGINE_CORE_PUBLIC_CONTAINER_ARRAY_H

#include "../Types/Aliases.h"

namespace AltinaEngine::Core::Container {

    // Fixed-size array, analogous to std::array.

    template <typename T, usize N> struct TArray {
        using TValueType      = T;
        using TSizeType       = usize;
        using TPointer        = TValueType*;
        using TConstPointer   = const TValueType*;
        using TReference      = TValueType&;
        using TConstReference = const TValueType&;
        using TIterator       = TValueType*;
        using TConstIterator  = const TValueType*;

        [[nodiscard]] static constexpr auto Size() noexcept -> TSizeType { return N; }

        [[nodiscard]] static constexpr auto IsEmpty() noexcept -> bool { return N == 0; }

        [[nodiscard]] constexpr auto        Data() noexcept -> TPointer { return mData; }

        [[nodiscard]] constexpr auto        Data() const noexcept -> TConstPointer { return mData; }

        [[nodiscard]] constexpr auto        operator[](TSizeType index) noexcept -> TReference {
            return mData[index];
        }

        [[nodiscard]] constexpr auto operator[](TSizeType index) const noexcept -> TConstReference {
            return mData[index];
        }

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
            return mData + N;
        } // NOLINT(*-identifier-naming)

        [[nodiscard]] auto end() const noexcept -> TConstIterator {
            return mData + N;
        } // NOLINT(*-identifier-naming)

        [[nodiscard]] auto cend() const noexcept -> TConstIterator {
            return mData + N;
        } // NOLINT(*-identifier-naming)

    private:
        TValueType mData[N > 0 ? N : 1]{}; // NOLINT
    };

} // namespace AltinaEngine::Core::Container

#endif // ALTINAENGINE_CORE_PUBLIC_CONTAINER_ARRAY_H
