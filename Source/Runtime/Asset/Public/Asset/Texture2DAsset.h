#pragma once

#include "Asset/AssetLoader.h"
#include "Asset/AssetTypes.h"
#include "Container/Vector.h"

namespace AltinaEngine::Asset {
    namespace Container = Core::Container;
    using Container::TVector;

    class AE_ASSET_API FTexture2DAsset final : public IAsset {
    public:
        FTexture2DAsset(FTexture2DDesc desc, TVector<u8> pixels);

        [[nodiscard]] auto GetDesc() const noexcept -> const FTexture2DDesc& { return mDesc; }
        [[nodiscard]] auto GetPixels() const noexcept -> const TVector<u8>& { return mPixels; }

    private:
        FTexture2DDesc mDesc{};
        TVector<u8>    mPixels;
    };

} // namespace AltinaEngine::Asset
