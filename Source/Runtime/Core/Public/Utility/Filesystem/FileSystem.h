#pragma once

#include "Base/CoreAPI.h"
#include "Container/Vector.h"
#include "Utility/Filesystem/Path.h"

namespace AltinaEngine::Core::Utility::Filesystem {
    using Container::TVector;

    struct AE_CORE_API FDirectoryEntry {
        FPath Path;
        bool  IsDirectory = false;
    };

    [[nodiscard]] AE_CORE_API auto Absolute(const FPath& path) -> FPath;
    [[nodiscard]] AE_CORE_API auto Relative(const FPath& path, const FPath& base) -> FPath;
    [[nodiscard]] AE_CORE_API auto IsDirectory(const FPath& path) -> bool;
    AE_CORE_API auto               EnumerateDirectory(
                      const FPath& root, bool recursive, TVector<FDirectoryEntry>& outEntries) -> bool;
} // namespace AltinaEngine::Core::Utility::Filesystem
