#pragma once

#include "AssetAPI.h"
#include "Asset/AssetManager.h"
#include "Asset/ModelAsset.h"
#include "Container/SmartPtr.h"

namespace AltinaEngine::Asset {
    struct AE_ASSET_API FModelAssetLoadResult {
        TShared<IAsset> mAsset{};
        FModelAsset*    mModel = nullptr;
    };

    class AE_ASSET_API FModelAssetLoader final {
    public:
        explicit FModelAssetLoader(FAssetManager& manager) : mManager(&manager) {}

        [[nodiscard]] auto Load(const FAssetHandle& handle) const -> FModelAssetLoadResult;

    private:
        FAssetManager* mManager = nullptr;
    };
} // namespace AltinaEngine::Asset
