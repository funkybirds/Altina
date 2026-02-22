#pragma once

#include "Asset/AssetTypes.h"

#include <vector>

namespace AltinaEngine::Tools::AssetPipeline {
    auto CookModel(const std::vector<u8>& sourceBytes, std::vector<u8>& outCooked,
        Asset::FModelDesc& outDesc) -> bool;
} // namespace AltinaEngine::Tools::AssetPipeline
