#pragma once

#include "Importers/Model/ModelImporter.h"

#include <filesystem>
#include <string>

namespace AltinaEngine::Tools::AssetPipeline {
    auto CookModelFromUsdz(const std::filesystem::path& sourcePath,
        const Asset::FAssetHandle& baseHandle, const std::string& baseVirtualPath,
        FModelCookResult& outResult, std::string& outError) -> bool;
} // namespace AltinaEngine::Tools::AssetPipeline
