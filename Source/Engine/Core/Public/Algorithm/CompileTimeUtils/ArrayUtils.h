#pragma once
#include "Container/Array.h"

namespace AltinaEngine::Core::Algorithm {
    using Container::TArray;

    template <usize S, usize E, typename T, usize N>
    consteval static auto GetSubArray(TArray<T, N> const& str) -> TArray<T, E - S> {
        TArray<T, E - S> subArray{};
        for (usize i = S; i < E; ++i) {
            subArray[i - S] = str[i];
        }
        return subArray;
    }

    template <typename T, usize N>
    consteval auto GetOccurrencePosition(TArray<T, N> const& str, T c, usize o) -> usize {
        for (usize i = 0; i < str.Size(); ++i) {
            if (str[i] == c) {
                if (o == 0)
                    return i;
                --o;
            }
        }
        return static_cast<usize>(-1);
    }

    template <typename T, usize N>
    consteval auto GetLastOccurrencePosition(TArray<T, N> const& str, T c) -> usize {
        usize pos = 0;
        for (usize i = 0; i < str.Size(); ++i) {
            if (str[i] == c)
                pos = i;
        }
        return pos;
    }

    template <usize S, usize N1, usize N2>
    consteval static auto HasPrefix(TArray<char, N1> const& str, TArray<char, N2> const& prefix)
        -> bool {
        for (usize i = 0; i < N2; ++i) {
            if (str[i + S] != prefix[i])
                return false;
        }
        return true;
    }

} // namespace AltinaEngine::Core::Algorithm
