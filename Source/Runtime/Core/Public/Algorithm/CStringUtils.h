#pragma once

#include "../Base/CoreAPI.h"
#include "Types/Traits.h"

namespace AltinaEngine::Core::Algorithm {

    namespace Details {
        AE_CORE_API auto StlToLowerW(wchar_t str) -> wchar_t;
        AE_CORE_API auto StlToUpperW(wchar_t str) -> wchar_t;
        AE_CORE_API auto StlToLowerA(char str) -> char;
        AE_CORE_API auto StlToUpperA(char str) -> char;
    } // namespace Details

    template <CCharType CharT> auto ToLowerChar(CharT Character) -> CharT {
        if constexpr (sizeof(CharT) == sizeof(wchar_t)) {
            return static_cast<CharT>(Details::StlToLowerW(static_cast<wchar_t>(Character)));
        } else {
            return static_cast<CharT>(Details::StlToLowerA(static_cast<char>(Character)));
        }
    }

    template <CCharType CharT> auto ToUpperChar(CharT Character) -> CharT {
        if constexpr (sizeof(CharT) == sizeof(wchar_t)) {
            return static_cast<CharT>(Details::StlToUpperW(static_cast<wchar_t>(Character)));
        } else {
            return static_cast<CharT>(Details::StlToUpperA(static_cast<char>(Character)));
        }
    }

    constexpr auto RawStringEqualUnsafe(const char* a, const char* b) -> bool {
        if (a == b) {
            return true;
        }
        if (a == nullptr || b == nullptr) {
            return false;
        }

        for (;;) {
            const char ca = *a;
            const char cb = *b;
            if (ca != cb) {
                return false;
            }
            if (ca == '\0' && cb == '\0') {
                return true;
            }
            ++a;
            ++b;
        }
    }

} // namespace AltinaEngine::Core::Algorithm
