#pragma once

#include "Asset/AssetTypes.h"
#include "AssetToolTypes.h"

#include <string>
#include <unordered_map>
#include <vector>

namespace AltinaEngine::Tools::AssetPipeline {
    auto CookMaterial(const std::vector<u8>&                        sourceBytes,
        const std::unordered_map<std::string, const FAssetRecord*>& assetsByPath,
        std::vector<Asset::FAssetHandle>& outDeps, Asset::FMaterialDesc& outDesc,
        std::vector<u8>& outCooked) -> bool;
} // namespace AltinaEngine::Tools::AssetPipeline
