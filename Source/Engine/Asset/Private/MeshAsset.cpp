#include "Asset/MeshAsset.h"

#include "Types/Traits.h"

namespace AltinaEngine::Asset {
    FMeshAsset::FMeshAsset(FMeshRuntimeDesc desc, TVector<FMeshVertexAttributeDesc> attributes,
        TVector<FMeshSubMeshDesc> subMeshes, TVector<u8> vertexData, TVector<u8> indexData)
        : mDesc(desc)
        , mAttributes(AltinaEngine::Move(attributes))
        , mSubMeshes(AltinaEngine::Move(subMeshes))
        , mVertexData(AltinaEngine::Move(vertexData))
        , mIndexData(AltinaEngine::Move(indexData)) {}

} // namespace AltinaEngine::Asset
