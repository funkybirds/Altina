#pragma once

#include "Rendering/RenderingAPI.h"

#include "Asset/MeshAsset.h"
#include "Geometry/StaticMeshData.h"

namespace AltinaEngine::Rendering {
    [[nodiscard]] auto AE_RENDERING_API ConvertMeshAssetToStaticMesh(
        const Asset::FMeshAsset& asset, RenderCore::Geometry::FStaticMeshData& outMesh) -> bool;
} // namespace AltinaEngine::Rendering
