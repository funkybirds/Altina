#pragma once

#include "Asset/AssetTypes.h"
#include "Importers/GeneratedAsset.h"

#include <filesystem>
#include <string>
#include <vector>

namespace AltinaEngine::Tools::AssetPipeline {
    struct FEnvMapCookResult {
        // Base cooked asset (stored as a regular Texture2D): RGBE-encoded equirectangular source.
        std::vector<u8>              CookedBytes;
        Asset::FTexture2DDesc        Desc{};

        // Derived products, currently emitted as Texture2D assets:
        // - Skybox cube faces (each face is a Texture2D with mip chain)
        // - Diffuse irradiance cube faces (each face is a Texture2D, mip=1)
        // - Specular prefiltered cube faces (each face is a Texture2D with mip chain)
        // - BRDF integration LUT (Texture2D)
        std::vector<FGeneratedAsset> Generated;

        // Additional stable bytes that should participate in cook key computation.
        std::vector<u8>              CookKeyExtras;
    };

    // HDRI route-B importer (tool-only): reads Radiance RGBE .hdr and offline-cooks IBL products.
    auto CookEnvMapFromHdr(const std::filesystem::path& sourcePath,
        const std::vector<u8>& sourceBytes, const Asset::FAssetHandle& baseHandle,
        const std::string& baseVirtualPath, FEnvMapCookResult& outResult, std::string& outError)
        -> bool;
} // namespace AltinaEngine::Tools::AssetPipeline
