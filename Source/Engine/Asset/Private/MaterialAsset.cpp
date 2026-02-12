#include "Asset/MaterialAsset.h"

#include "Types/Traits.h"

namespace AltinaEngine::Asset {
    FMaterialAsset::FMaterialAsset(FMaterialRuntimeDesc desc, TVector<FMaterialScalarParam> scalars,
        TVector<FMaterialVectorParam> vectors, TVector<FMaterialTextureParam> textures)
        : mDesc(desc)
        , mScalars(AltinaEngine::Move(scalars))
        , mVectors(AltinaEngine::Move(vectors))
        , mTextures(AltinaEngine::Move(textures)) {}

} // namespace AltinaEngine::Asset
