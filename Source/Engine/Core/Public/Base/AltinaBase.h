#pragma once

// Platform detection macros (0 or 1)
#if defined(_WIN32) || defined(_WIN64)
    #define AE_PLATFORM_WIN 1 // NOLINT
#else
    #define AE_PLATFORM_WIN 0 // NOLINT
#endif

#if defined(__APPLE__) && defined(__MACH__)
    #define AE_PLATFORM_MACOS 1 // NOLINT
#else
    #define AE_PLATFORM_MACOS 0 // NOLINT
#endif

#if defined(__ANDROID__)
    #define AE_PLATFORM_ANDROID 1 // NOLINT
#else
    #define AE_PLATFORM_ANDROID 0 // NOLINT
#endif

#if defined(__linux__) && !defined(__ANDROID__)
    #define AE_PLATFORM_LINUX 1 // NOLINT
#else
    #define AE_PLATFORM_LINUX 0 // NOLINT
#endif

// Export / import and force-inline macros
#if AE_PLATFORM_WIN
    #define AE_DLLEXPORT __declspec(dllexport)
    #define AE_DLLIMPORT __declspec(dllimport)
    #define AE_FORCEINLINE __forceinline
#elif defined(__GNUC__) || defined(__clang__)
    #define AE_DLLEXPORT __attribute__((visibility("default")))
    #define AE_DLLIMPORT
    #define AE_FORCEINLINE inline __attribute__((always_inline))
#else
    #error "Unknown compiler, please define AE_DLLEXPORT, AE_DLLIMPORT, and AE_FORCEINLINE for your compiler"
#endif