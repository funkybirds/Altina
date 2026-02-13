#pragma once

#include "Asset/AssetTypes.h"

namespace AltinaEngine::Asset {
    namespace Container = Core::Container;
    using Container::FNativeString;
    using Container::FNativeStringView;
    using Container::FString;
    using Container::FStringView;
    using Container::TVector;

    class AE_ASSET_API FAssetRegistry {
    public:
        [[nodiscard]] auto LoadFromJsonFile(const FString& path) -> bool;
        [[nodiscard]] auto LoadFromJsonText(FNativeStringView text) -> bool;
        [[nodiscard]] auto GetLastError() const noexcept -> FNativeStringView;

        void               Clear();
        void               AddAsset(FAssetDesc desc);
        void               AddRedirector(FAssetRedirector redirector);

        [[nodiscard]] auto FindByPath(FStringView path) const noexcept -> FAssetHandle;
        [[nodiscard]] auto FindByUuid(const FUuid& uuid) const noexcept -> FAssetHandle;
        [[nodiscard]] auto GetDesc(const FAssetHandle& handle) const noexcept -> const FAssetDesc*;
        [[nodiscard]] auto GetDependencies(const FAssetHandle& handle) const noexcept
            -> const TVector<FAssetHandle>*;
        [[nodiscard]] auto ResolveRedirector(const FAssetHandle& handle) const noexcept
            -> FAssetHandle;

    private:
        TVector<FAssetDesc>       mAssets;
        TVector<FAssetRedirector> mRedirectors;
        FNativeString             mLastError;
    };

} // namespace AltinaEngine::Asset
