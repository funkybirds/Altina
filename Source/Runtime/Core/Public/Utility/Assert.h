#pragma once
#include "Logging/Log.h"
namespace AltinaEngine::Core::Utility {
    using AltinaEngine::Forward;
    using Core::Container::FStringView;

#if !defined(AE_ENABLE_DEBUG_ASSERT)
    // Global switch for DebugAssert. Override with -DAE_ENABLE_DEBUG_ASSERT=0/1.
    #define AE_ENABLE_DEBUG_ASSERT 1
#endif

    inline constexpr bool kEnableDebugAssert = (AE_ENABLE_DEBUG_ASSERT != 0);

    template <typename... Args>
    void Assert(
        bool condition, FStringView category, TFormatString<Args...> format, Args&&... args) {
        if (!condition) [[unlikely]] {
            LogErrorCat(category, format, Forward<Args>(args)...);
            std::abort();
        }
    }

    template <typename... Args>
    void DebugAssert(
        bool condition, FStringView category, TFormatString<Args...> format, Args&&... args) {
        if constexpr (kEnableDebugAssert) {
            if (!condition) [[unlikely]] {
                LogErrorCat(category, format, Forward<Args>(args)...);
                std::abort();
            }
        } else {
            (void)condition;
            (void)category;
            (void)format;
            ((void)args, ...);
        }
    }
} // namespace AltinaEngine::Core::Utility
