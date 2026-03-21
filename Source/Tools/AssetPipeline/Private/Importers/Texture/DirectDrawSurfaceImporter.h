#pragma once

#include "Asset/AssetTypes.h"

#include <string>
#include <vector>

namespace AltinaEngine::Tools::AssetPipeline {
    auto CookDirectDrawSurfaceTexture2D(const std::vector<u8>& sourceBytes, bool srgb,
        std::vector<u8>& outCooked, Asset::FTexture2DDesc& outDesc) -> bool;

    auto RunDirectDrawSurfaceSelfTest(std::string& outError) -> bool;
} // namespace AltinaEngine::Tools::AssetPipeline
