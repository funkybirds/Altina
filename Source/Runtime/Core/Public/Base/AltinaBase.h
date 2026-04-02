#pragma once

// Compiler detection macros
// Detect MSVC, Clang, and GCC
#if defined(_MSC_VER)
    #define AE_COMPILER_MSVC 1 // NOLINT
#else
    #define AE_COMPILER_MSVC 0 // NOLINT
#endif

#if defined(__clang__)
    #define AE_COMPILER_CLANG 1 // NOLINT
#else
    #define AE_COMPILER_CLANG 0 // NOLINT
#endif

#if defined(__GNUC__) && !defined(__clang__)
    #define AE_COMPILER_GCC 1 // NOLINT
#else
    #define AE_COMPILER_GCC 0 // NOLINT
#endif

// C++ standard detection macros
#if defined(_MSVC_LANG)
    #define AE_CPLUSPLUS _MSVC_LANG // NOLINT
#else
    #define AE_CPLUSPLUS __cplusplus // NOLINT
#endif

#define AE_CPP_11_OR_LATER (AE_CPLUSPLUS >= 201103L)                  // NOLINT
#define AE_CPP_14_OR_LATER (AE_CPLUSPLUS >= 201402L)                  // NOLINT
#define AE_CPP_17_OR_LATER (AE_CPLUSPLUS >= 201703L)                  // NOLINT
#define AE_CPP_20_OR_LATER (AE_CPLUSPLUS >= 202002L)                  // NOLINT
#define AE_CPP_23_OR_LATER (AE_CPLUSPLUS >= 202100L)                  // NOLINT
#define AE_CPP_26_OR_LATER (AE_CPLUSPLUS >= 202400L) && (!_MSVC_LANG) // NOLINT

#if !AE_CPP_23_OR_LATER
    #error "AltinaEngine requires C++23 or later."
#endif

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
    #error \
        "Unknown compiler, please define AE_DLLEXPORT, AE_DLLIMPORT, and AE_FORCEINLINE for your compiler"
#endif

#if AE_CPP_26_OR_LATER
    #define AE_FUNC_DELETE(reason) delete (msg)
#else
    #define AE_FUNC_DELETE(reason) delete
#endif