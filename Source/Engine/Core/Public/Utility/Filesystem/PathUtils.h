#pragma once

#include "Utility/Filesystem/Path.h"

namespace AltinaEngine::Core::Utility::Filesystem {
    [[nodiscard]] inline auto GetCurrentWorkingDir() -> FPath {
        return FPath(Core::Platform::GetCurrentWorkingDir());
    }

    inline auto SetCurrentWorkingDir(const FPath& path) -> bool {
        return Core::Platform::SetCurrentWorkingDir(path.GetString());
    }

    [[nodiscard]] inline auto GetTempDirectory() -> FPath {
        return FPath(Core::Platform::GetTempDirectory());
    }
} // namespace AltinaEngine::Core::Utility::Filesystem
