#pragma once

#include "Asset/AssetTypes.h"
#include "Types/Aliases.h"

#include <filesystem>
#include <vector>

namespace AltinaEngine::Tools::AssetPipeline {
    auto CookMesh(const std::filesystem::path& sourcePath, std::vector<u8>& outCooked,
        Asset::FMeshDesc& outDesc, std::vector<u8>& outCookKeyBytes) -> bool;
} // namespace AltinaEngine::Tools::AssetPipeline
