#include "Platform/PlatformDynamicLibrary.h"

#include "Utility/String/CodeConvert.h"

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
#else
    #include <dlfcn.h>
#endif

namespace AltinaEngine::Core::Platform {
    auto LoadDynamicLibrary(const FString& path) -> FDynamicLibraryHandle {
        if (path.IsEmptyString()) {
            return nullptr;
        }

#if AE_PLATFORM_WIN
    #if defined(AE_UNICODE) || defined(UNICODE) || defined(_UNICODE)
        return static_cast<FDynamicLibraryHandle>(LoadLibraryW(path.CStr()));
    #else
        return static_cast<FDynamicLibraryHandle>(LoadLibraryA(path.CStr()));
    #endif
#else
        const auto utf8Path = Utility::String::ToUtf8Bytes(path);
        return dlopen(utf8Path.CStr(), RTLD_NOW | RTLD_LOCAL);
#endif
    }

    void UnloadDynamicLibrary(FDynamicLibraryHandle handle) {
        if (handle == nullptr) {
            return;
        }

#if AE_PLATFORM_WIN
        FreeLibrary(static_cast<HMODULE>(handle));
#else
        dlclose(handle);
#endif
    }

    auto GetDynamicLibrarySymbol(FDynamicLibraryHandle handle, const char* symbolName) -> void* {
        if (handle == nullptr || symbolName == nullptr || symbolName[0] == '\0') {
            return nullptr;
        }

#if AE_PLATFORM_WIN
        return reinterpret_cast<void*>(GetProcAddress(static_cast<HMODULE>(handle), symbolName));
#else
        return dlsym(handle, symbolName);
#endif
    }

    auto GetDynamicLibraryExtension() -> const TChar* {
#if AE_PLATFORM_WIN
        return TEXT(".dll");
#elif AE_PLATFORM_MACOS
        return TEXT(".dylib");
#else
        return TEXT(".so");
#endif
    }
} // namespace AltinaEngine::Core::Platform
