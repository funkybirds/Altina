#include "Asset/MaterialAsset.h"

#include "Types/Traits.h"

using AltinaEngine::Move;
namespace AltinaEngine::Asset {
    FMaterialAsset::FMaterialAsset(FString name, TVector<FMaterialPassTemplate> passes,
        TVector<TVector<FString>> precompileVariants)
        : mName(Move(name))
        , mPasses(Move(passes))
        , mPrecompileVariants(Move(precompileVariants)) {}

} // namespace AltinaEngine::Asset
