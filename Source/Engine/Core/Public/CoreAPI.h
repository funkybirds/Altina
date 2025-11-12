#pragma once

#if defined(_WIN32)
    #if defined(AE_CORE_BUILD)
        #define AE_CORE_API __declspec(dllexport)
    #else
        #define AE_CORE_API __declspec(dllimport)
    #endif
#elif defined(__GNUC__) || defined(__clang__)
    #define AE_CORE_API __attribute__((visibility("default")))
#else
    #define AE_CORE_API
#endif
