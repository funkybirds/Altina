#include "Algorithm/CStringUtils.h"

#include <cctype>
#include <cwctype>

namespace AltinaEngine::Core::Algorithm::Details {
    auto StlToLowerW(wchar_t ch) -> wchar_t {
        return static_cast<wchar_t>(std::towlower(static_cast<wint_t>(ch)));
    }

    auto StlToUpperW(wchar_t ch) -> wchar_t {
        return static_cast<wchar_t>(std::towupper(static_cast<wint_t>(ch)));
    }

    auto StlToLowerA(char ch) -> char {
        // std::tolower/std::toupper are undefined for negative values other than EOF.
        const auto u = static_cast<unsigned char>(ch);
        return static_cast<char>(std::tolower(u));
    }

    auto StlToUpperA(char ch) -> char {
        const auto u = static_cast<unsigned char>(ch);
        return static_cast<char>(std::toupper(u));
    }
} // namespace AltinaEngine::Core::Algorithm::Details
