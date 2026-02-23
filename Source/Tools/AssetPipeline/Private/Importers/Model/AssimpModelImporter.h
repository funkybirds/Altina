#pragma once

#include "Importers/Model/ModelImporter.h"

#include <filesystem>
#include <string>

struct aiScene;

namespace AltinaEngine::Tools::AssetPipeline {
    // Shared implementation that cooks our Model blob (plus generated Mesh/Material/Texture assets)
    // from an already-loaded Assimp scene. Format-specific importer front-ends (FBX/PMX) load the
    // file into an aiScene and then delegate here.
    auto CookModelFromAssimpScene(const std::filesystem::path& sourcePath, const aiScene* scene,
        const Asset::FAssetHandle& baseHandle, const std::string& baseVirtualPath,
        FModelCookResult& outResult, std::string& outError) -> bool;
} // namespace AltinaEngine::Tools::AssetPipeline
