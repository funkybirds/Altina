#pragma once

#include "Asset/AssetTypes.h"

#include <filesystem>
#include <vector>

namespace AltinaEngine::Tools::AssetPipeline {
    auto CookAudio(const std::filesystem::path& sourcePath, const std::vector<u8>& sourceBytes,
        std::vector<u8>& outCooked, Asset::FAudioDesc& outDesc) -> bool;
} // namespace AltinaEngine::Tools::AssetPipeline
