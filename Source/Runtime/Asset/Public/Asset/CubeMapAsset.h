#pragma once

#include "Asset/AssetLoader.h"
#include "Asset/AssetTypes.h"
#include "Container/Vector.h"

namespace AltinaEngine::Asset {
    namespace Container = Core::Container;
    using Container::TVector;

    // Cooked cube map texture asset (6 faces, tightly-packed mip chain).
    // Pixel layout: mip-major, face-minor:
    //   for mip in [0..MipCount): for face in [0..5): data(face,mip)
    class AE_ASSET_API FCubeMapAsset final : public IAsset {
    public:
        FCubeMapAsset(FCubeMapDesc desc, TVector<u8> pixels);

        [[nodiscard]] auto GetDesc() const noexcept -> const FCubeMapDesc& { return mDesc; }
        [[nodiscard]] auto GetPixels() const noexcept -> const TVector<u8>& { return mPixels; }

    private:
        FCubeMapDesc mDesc{};
        TVector<u8>  mPixels;
    };
} // namespace AltinaEngine::Asset
