#pragma once

#include "../Base/CoreAPI.h"
#include "../Container/String.h"
#include "../Container/StringView.h"
#include "../Container/Vector.h"
#include "../Types/Aliases.h"

namespace AltinaEngine::Core::Platform {
    using Container::FNativeString;
    using Container::FString;
    using Container::FStringView;
    using Container::TVector;

    AE_CORE_API auto ReadFileBytes(const FString& path, TVector<u8>& outBytes) -> bool;
    AE_CORE_API auto ReadFileTextUtf8(const FString& path, FNativeString& outText) -> bool;
    AE_CORE_API void RemoveFileIfExists(const FString& path);
    AE_CORE_API auto GetExecutableDir() -> FString;
    AE_CORE_API auto GetCurrentWorkingDir() -> FString;
    AE_CORE_API auto SetCurrentWorkingDir(const FString& path) -> bool;
    AE_CORE_API auto GetTempDirectory() -> FString;
    AE_CORE_API auto CreateDirectories(const FString& path) -> bool;
    AE_CORE_API auto IsPathExist(const FString& path) -> bool;
    AE_CORE_API auto GetPathSeparator() -> TChar;
    AE_CORE_API auto IsPathSeparator(TChar value) -> bool;
    AE_CORE_API auto IsAbsolutePath(FStringView path) -> bool;
    AE_CORE_API auto NormalizePath(FStringView path) -> FString;
    AE_CORE_API auto GetRootLength(FStringView path) -> usize;
} // namespace AltinaEngine::Core::Platform
