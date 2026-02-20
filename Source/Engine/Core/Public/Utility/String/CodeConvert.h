#pragma once

#include "Base/AltinaBase.h"
#include "Container/String.h"
#include "Types/Aliases.h"

#include <string>

#if AE_PLATFORM_WIN
    #ifdef TEXT
        #undef TEXT
    #endif
    #ifndef WIN32_LEAN_AND_MEAN
        #define WIN32_LEAN_AND_MEAN
    #endif
    #ifndef NOMINMAX
        #define NOMINMAX
    #endif
    #include <windows.h>
    #ifdef TEXT
        #undef TEXT
    #endif
    #if defined(AE_UNICODE) || defined(UNICODE) || defined(_UNICODE)
        #define TEXT(str) L##str
    #else
        #define TEXT(str) str
    #endif
#endif

namespace AltinaEngine::Core::Utility::String {
    [[nodiscard]] inline auto FromUtf8(const Core::Container::FNativeString& value)
        -> Core::Container::FString {
        Core::Container::FString out;
        if (value.IsEmptyString()) {
            return out;
        }
#if defined(AE_UNICODE) || defined(UNICODE) || defined(_UNICODE)
    #if AE_PLATFORM_WIN
        int wideCount = MultiByteToWideChar(
            CP_UTF8, 0, value.GetData(), static_cast<int>(value.Length()), nullptr, 0);
        if (wideCount <= 0) {
            return out;
        }
        std::wstring wide(static_cast<size_t>(wideCount), L'\0');
        MultiByteToWideChar(
            CP_UTF8, 0, value.GetData(), static_cast<int>(value.Length()), wide.data(), wideCount);
        out.Append(wide.c_str(), wide.size());
    #else
        out.Append(value.GetData(), value.Length());
    #endif
#else
        out.Append(value.GetData(), value.Length());
#endif
        return out;
    }
} // namespace AltinaEngine::Core::Utility::String
