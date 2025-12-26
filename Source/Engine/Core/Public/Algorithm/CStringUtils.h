#pragma once

#include <cctype>
#include <cwctype>
#include "../Types/Concepts.h"

namespace AltinaEngine::Core::Algorithm
{
    template <AltinaEngine::ICharType CharT>
    inline CharT ToLowerChar(CharT Character)
    {
        if constexpr (sizeof(CharT) == sizeof(wchar_t))
        {
            return static_cast<CharT>(std::towlower(static_cast<wint_t>(Character)));
        }
        else
        {
            const unsigned char Narrow = static_cast<unsigned char>(Character);
            return static_cast<CharT>(std::tolower(Narrow));
        }
    }

    template <AltinaEngine::ICharType CharT>
    inline CharT ToUpperChar(CharT Character)
    {
        if constexpr (sizeof(CharT) == sizeof(wchar_t))
        {
            return static_cast<CharT>(std::towupper(static_cast<wint_t>(Character)));
        }
        else
        {
            const unsigned char Narrow = static_cast<unsigned char>(Character);
            return static_cast<CharT>(std::toupper(Narrow));
        }
    }

} // namespace AltinaEngine::Core::Algorithm
