#pragma once

#include "Asset/AssetTypes.h"

#include <vector>

namespace AltinaEngine::Tools::AssetPipeline {
    auto CookTexture2D(const std::vector<u8>& sourceBytes, bool srgb, std::vector<u8>& outCooked,
        Asset::FTexture2DDesc& outDesc) -> bool;
} // namespace AltinaEngine::Tools::AssetPipeline
