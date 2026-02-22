#pragma once

#include "Asset/AssetTypes.h"

#include <filesystem>
#include <vector>

namespace AltinaEngine::Tools::AssetPipeline {
    auto CookShader(const std::filesystem::path& sourcePath, const std::vector<u8>& sourceBytes,
        const std::filesystem::path& repoRoot, std::vector<u8>& outCooked,
        Asset::FShaderDesc& outDesc) -> bool;
} // namespace AltinaEngine::Tools::AssetPipeline
