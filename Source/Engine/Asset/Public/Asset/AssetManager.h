#pragma once

#include "Asset/AssetLoader.h"
#include "Asset/AssetRegistry.h"
#include "Container/SmartPtr.h"
#include "Container/Vector.h"

namespace AltinaEngine::Asset {
    using Core::Container::TShared;
    using Core::Container::TVector;

    class AE_ASSET_API FAssetManager {
    public:
        FAssetManager();

        void SetRegistry(const FAssetRegistry* registry) noexcept;

        void RegisterLoader(IAssetLoader* loader);
        void UnregisterLoader(IAssetLoader* loader);

        [[nodiscard]] auto Load(const FAssetHandle& handle) -> TShared<IAsset>;
        void               Unload(const FAssetHandle& handle);
        void               ClearCache();

        [[nodiscard]] auto FindLoaded(const FAssetHandle& handle) const -> TShared<IAsset>;

    private:
        struct FCacheEntry {
            FAssetHandle   Handle;
            TShared<IAsset> Asset;
        };

        [[nodiscard]] auto FindLoader(EAssetType type) const noexcept -> IAssetLoader*;
        [[nodiscard]] auto FindCacheIndex(const FAssetHandle& handle) const noexcept -> isize;

        const FAssetRegistry* mRegistry = nullptr;
        TVector<IAssetLoader*> mLoaders;
        TVector<FCacheEntry>   mCache;
    };

} // namespace AltinaEngine::Asset
