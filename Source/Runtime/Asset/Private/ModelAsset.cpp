#include "Asset/ModelAsset.h"

using AltinaEngine::Move;
namespace AltinaEngine::Asset {
    FModelAsset::FModelAsset(FModelRuntimeDesc desc, TVector<FModelNodeDesc> nodes,
        TVector<FModelMeshRef> meshRefs, TVector<FAssetHandle> materialSlots)
        : mDesc(desc)
        , mNodes(Move(nodes))
        , mMeshRefs(Move(meshRefs))
        , mMaterialSlots(Move(materialSlots)) {}

} // namespace AltinaEngine::Asset
