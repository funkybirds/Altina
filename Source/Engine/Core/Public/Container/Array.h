#ifndef ALTINAENGINE_CORE_PUBLIC_CONTAINER_ARRAY_H
#define ALTINAENGINE_CORE_PUBLIC_CONTAINER_ARRAY_H

#include "../Base/CoreAPI.h"
#include "../Types/Aliases.h"

namespace AltinaEngine::Core::Container
{

    // Fixed-size array, analogous to std::array.
    
    
    template <typename T, usize N>
    struct TArray
    {
        using value_type      = T;
        using size_type       = usize;
        using pointer         = value_type*;
        using const_pointer   = const value_type*;
        using reference       = value_type&;
        using const_reference = const value_type&;
        using iterator        = value_type*;
        using const_iterator  = const value_type*;

        // Capacity / state
        [[nodiscard]] static constexpr size_type Size() noexcept { return N; }

        [[nodiscard]] static constexpr bool IsEmpty() noexcept { return N == 0; }

        // Data access
        [[nodiscard]] pointer Data() noexcept { return mData; }

        [[nodiscard]] const_pointer Data() const noexcept { return mData; }

        [[nodiscard]] reference operator[](size_type index) noexcept { return mData[index]; }

        [[nodiscard]] const_reference operator[](size_type index) const noexcept { return mData[index]; }

        // Iteration (std::array-compatible names)
        [[nodiscard]] iterator begin() noexcept { return mData; }

        [[nodiscard]] const_iterator begin() const noexcept { return mData; }

        [[nodiscard]] const_iterator cbegin() const noexcept { return mData; }

        [[nodiscard]] iterator end() noexcept { return mData + N; }

        [[nodiscard]] const_iterator end() const noexcept { return mData + N; }

        [[nodiscard]] const_iterator cend() const noexcept { return mData + N; }

    private:
        value_type mData[N > 0 ? N : 1]{};
    };

} // namespace AltinaEngine::Core::Container

#endif // ALTINAENGINE_CORE_PUBLIC_CONTAINER_ARRAY_H
