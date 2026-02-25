#include "Asset/CubeMapAsset.h"

using AltinaEngine::Move;
namespace AltinaEngine::Asset {
    FCubeMapAsset::FCubeMapAsset(FCubeMapDesc desc, TVector<u8> pixels)
        : mDesc(Move(desc)), mPixels(Move(pixels)) {}
} // namespace AltinaEngine::Asset
