#pragma once

#include "Importers/Mesh/MeshBuild.h"

#include <filesystem>
#include <vector>

namespace AltinaEngine::Tools::AssetPipeline {
    auto CookMeshFromGltf(const std::filesystem::path& sourcePath, FMeshBuildResult& outMesh,
        std::vector<u8>& outCookKeyBytes) -> bool;
} // namespace AltinaEngine::Tools::AssetPipeline
