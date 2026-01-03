#pragma once
#include "Container/Array.h"

namespace AltinaEngine::Core::Algorithm {
    using Container::TArray;

    template <unsigned S, unsigned E, typename T, unsigned N>
    consteval static auto GetSubArray(TArray<T, N> const& str) -> TArray<T, E - S> {
        TArray<T, E - S> subArray{};
        for (unsigned i = S; i < E; ++i) {
            subArray[i - S] = str[i];
        }
        return subArray;
    }

    template <typename T, unsigned N>
    consteval auto GetOccurrencePosition(TArray<T, N> const& str, T c, u64 o) -> u64 {
        for (u64 i = 0; i < str.Size(); ++i) {
            if (str[i] == c) {
                if (o == 0)
                    return i;
                --o;
            }
        }
        return ~0u;
    }

    template <typename T, unsigned N>
    consteval auto GetLastOccurrencePosition(TArray<T, N> const& str, T c) -> u64 {
        u64 pos = 0;
        for (u64 i = 0; i < str.Size(); ++i) {
            if (str[i] == c)
                pos = i;
        }
        return pos;
    }

    template <unsigned S, unsigned N1, unsigned N2>
    consteval static auto HasPrefix(TArray<char, N1> const& str, TArray<char, N2> const& prefix)
        -> bool {
        for (unsigned i = 0; i < N2; ++i) {
            if (str[i + S] != prefix[i])
                return false;
        }
        return true;
    }

} // namespace AltinaEngine::Core::Algorithm