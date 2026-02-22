#pragma once

#include "Container/String.h"
#include "Container/StringView.h"
#include "Utility/Uuid.h"

namespace AltinaEngine::Core::Utility::String {
    [[nodiscard]] inline auto ParseUuid(const Core::Container::FNativeString& text,
        FUuid& out) -> bool {
        if (text.IsEmptyString()) {
            return false;
        }
        return FUuid::TryParse(Core::Container::FNativeStringView(text.GetData(), text.Length()),
            out);
    }
} // namespace AltinaEngine::Core::Utility::String
