#pragma once

#include "Asset/AssetTypes.h"

#include <filesystem>
#include <vector>

namespace AltinaEngine::Tools::AssetPipeline {
    auto CookLevel(const std::filesystem::path& sourcePath, const std::vector<u8>& sourceBytes,
        std::vector<u8>& outCooked, std::vector<Asset::FAssetHandle>& outDependencies,
        Asset::FLevelDesc& outDesc) -> bool;
} // namespace AltinaEngine::Tools::AssetPipeline
