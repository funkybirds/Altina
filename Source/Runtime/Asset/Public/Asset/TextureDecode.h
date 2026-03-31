#pragma once

#include "Asset/AssetTypes.h"
#include "Asset/Texture2DAsset.h"

namespace AltinaEngine::Asset {
    AE_ASSET_API auto DecodeTexture2DToRgba8(
        const FTexture2DAsset& asset, FTexture2DDesc& outDesc, TVector<u8>& outPixels) -> bool;
}
