#pragma once

#include "Asset/AssetTypes.h"

#include <string>
#include <vector>

namespace AltinaEngine::Tools::AssetPipeline {
    // Shared "cooked side products" container used by importers that generate multiple assets
    // from a single source (e.g. model -> meshes/materials, hdr -> envmap products).
    struct FGeneratedAsset {
        Asset::FAssetHandle              Handle{};
        Asset::EAssetType                Type = Asset::EAssetType::Unknown;
        std::string                      VirtualPath;
        std::vector<u8>                  CookedBytes;
        std::vector<Asset::FAssetHandle> Dependencies;
        Asset::FTexture2DDesc            TextureDesc{};
        Asset::FMeshDesc                 MeshDesc{};
        Asset::FMaterialDesc             MaterialDesc{};
    };
} // namespace AltinaEngine::Tools::AssetPipeline
