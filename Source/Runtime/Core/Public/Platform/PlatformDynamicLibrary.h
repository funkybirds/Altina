#pragma once

#include "Base/CoreAPI.h"
#include "Container/String.h"

namespace AltinaEngine::Core::Platform {
    using Container::FString;

    using FDynamicLibraryHandle = void*;

    AE_CORE_API auto LoadDynamicLibrary(const FString& path) -> FDynamicLibraryHandle;
    AE_CORE_API void UnloadDynamicLibrary(FDynamicLibraryHandle handle);
    AE_CORE_API auto GetDynamicLibrarySymbol(FDynamicLibraryHandle handle, const char* symbolName)
        -> void*;
    AE_CORE_API auto GetDynamicLibraryExtension() -> const TChar*;
} // namespace AltinaEngine::Core::Platform
