#include "Asset/MeshAsset.h"

#include "Types/Traits.h"

using AltinaEngine::Move;
namespace AltinaEngine::Asset {
    FMeshAsset::FMeshAsset(FMeshRuntimeDesc desc, TVector<FMeshVertexAttributeDesc> attributes,
        TVector<FMeshSubMeshDesc> subMeshes, TVector<u8> vertexData, TVector<u8> indexData)
        : mDesc(desc)
        , mAttributes(Move(attributes))
        , mSubMeshes(Move(subMeshes))
        , mVertexData(Move(vertexData))
        , mIndexData(Move(indexData)) {}

} // namespace AltinaEngine::Asset
