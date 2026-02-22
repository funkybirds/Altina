#pragma once

#include "Asset/AssetLoader.h"

namespace AltinaEngine::Asset {
    class AE_ASSET_API FScriptLoader final : public IAssetLoader {
    public:
        [[nodiscard]] auto CanLoad(EAssetType type) const noexcept -> bool override;
        auto Load(const FAssetDesc& desc, IAssetStream& stream) -> TShared<IAsset> override;
    };
} // namespace AltinaEngine::Asset
