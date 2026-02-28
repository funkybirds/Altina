#pragma once
#include "Logging/Log.h"
namespace AltinaEngine::Core::Utility {
    using Core::Container::FStringView;

    template <typename... Args>
    void Assert(
        bool condition, FStringView category, TFormatString<Args...> format, Args&&... args) {
        if (!condition) [[unlikely]] {
            LogErrorCat(category, format, args...);
            std::abort();
        }
    }

    template <typename... Args>
    void DebugAssert(
        bool condition, FStringView category, TFormatString<Args...> format, Args&&... args) {
        (void)condition;
        (void)category;
        (void)format;
        ((void)args, ...);
    }
} // namespace AltinaEngine::Core::Utility
