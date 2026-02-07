#pragma once

#include "../Base/CoreAPI.h"
#include "../Container/String.h"
#include "../Container/Vector.h"
#include "../Types/Aliases.h"

namespace AltinaEngine::Core::Platform {
    using Container::FNativeString;
    using Container::FString;
    using Container::TVector;

    AE_CORE_API auto ReadFileBytes(const FString& path, TVector<u8>& outBytes) -> bool;
    AE_CORE_API auto ReadFileTextUtf8(const FString& path, FNativeString& outText) -> bool;
    AE_CORE_API void RemoveFileIfExists(const FString& path);
} // namespace AltinaEngine::Core::Platform
