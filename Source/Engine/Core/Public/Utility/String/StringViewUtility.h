#pragma once

#include "Algorithm/CStringUtils.h"
#include "Container/StringView.h"
#include "Types/Aliases.h"

#include <cstring>

namespace AltinaEngine::Core::Utility::String {
    [[nodiscard]] inline auto EqualLiteralI(Core::Container::FNativeStringView text,
        const char* literal) -> bool {
        if (literal == nullptr) {
            return false;
        }
        const usize length = static_cast<usize>(std::strlen(literal));
        if (text.Length() != length) {
            return false;
        }
        for (usize i = 0; i < length; ++i) {
            if (Core::Algorithm::ToLowerChar(text[i])
                != Core::Algorithm::ToLowerChar(literal[i])) {
                return false;
            }
        }
        return true;
    }

    [[nodiscard]] inline auto EqualsIgnoreCase(Core::Container::FStringView left,
        Core::Container::FStringView right) -> bool {
        if (left.Length() != right.Length()) {
            return false;
        }

        for (usize i = 0; i < left.Length(); ++i) {
            if (Core::Algorithm::ToLowerChar(left[i]) != Core::Algorithm::ToLowerChar(right[i])) {
                return false;
            }
        }

        return true;
    }
} // namespace AltinaEngine::Core::Utility::String
