#pragma once

#include "Asset/AssetTypes.h"

#include <vector>
#include <string>
#include <filesystem>

namespace AltinaEngine::Tools::AssetPipeline {
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

    struct FModelCookResult {
        std::vector<u8>                  CookedBytes;
        Asset::FModelDesc                Desc{};
        std::vector<u8>                  CookKeyExtras;
        std::vector<FGeneratedAsset>     Generated;
        std::vector<Asset::FAssetHandle> ModelDependencies;
    };

    auto CookModel(const std::filesystem::path& sourcePath, const std::vector<u8>& sourceBytes,
        const Asset::FAssetHandle& baseHandle, const std::string& baseVirtualPath,
        FModelCookResult& outResult, std::string& outError) -> bool;
} // namespace AltinaEngine::Tools::AssetPipeline
