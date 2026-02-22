#pragma once

#include <cctype>
#include <cwctype>
#include "../Types/Concepts.h"

namespace AltinaEngine::Core::Algorithm {
    template <CCharType CharT> auto ToLowerChar(CharT Character) -> CharT {
        if constexpr (sizeof(CharT) == sizeof(wchar_t)) {
            return static_cast<CharT>(std::towlower(static_cast<wint_t>(Character)));
        } else {
            const auto narrow = static_cast<unsigned char>(Character);
            return static_cast<CharT>(std::tolower(narrow));
        }
    }

    template <CCharType CharT> auto ToUpperChar(CharT Character) -> CharT {
        if constexpr (sizeof(CharT) == sizeof(wchar_t)) {
            return static_cast<CharT>(std::towupper(static_cast<wint_t>(Character)));
        } else {
            const auto narrow = static_cast<unsigned char>(Character);
            return static_cast<CharT>(std::toupper(narrow));
        }
    }

} // namespace AltinaEngine::Core::Algorithm
