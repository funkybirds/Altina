#include "Asset/MaterialAsset.h"

#include "Types/Traits.h"

using AltinaEngine::Move;
namespace AltinaEngine::Asset {
    FMaterialAsset::FMaterialAsset(FMaterialRuntimeDesc desc, TVector<FMaterialScalarParam> scalars,
        TVector<FMaterialVectorParam> vectors, TVector<FMaterialTextureParam> textures)
        : mDesc(desc)
        , mScalars(Move(scalars))
        , mVectors(Move(vectors))
        , mTextures(Move(textures)) {}

} // namespace AltinaEngine::Asset
