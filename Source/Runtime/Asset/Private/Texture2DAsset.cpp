#include "Asset/Texture2DAsset.h"

#include "Types/Traits.h"

using AltinaEngine::Move;
namespace AltinaEngine::Asset {
    FTexture2DAsset::FTexture2DAsset(FTexture2DDesc desc, TVector<u8> pixels)
        : mDesc(desc), mPixels(Move(pixels)) {}

} // namespace AltinaEngine::Asset
