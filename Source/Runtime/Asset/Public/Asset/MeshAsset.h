#pragma once

#include "Asset/AssetBinary.h"
#include "Asset/AssetLoader.h"
#include "Container/Vector.h"

namespace AltinaEngine::Asset {
    namespace Container = Core::Container;
    using Container::TVector;

    struct AE_ASSET_API FMeshRuntimeDesc {
        u32 mVertexCount  = 0;
        u32 mIndexCount   = 0;
        u32 mVertexStride = 0;
        u32 mIndexType    = 0;
        u32 mFlags        = 0;
        f32 mBoundsMin[3] = { 0.0f, 0.0f, 0.0f };
        f32 mBoundsMax[3] = { 0.0f, 0.0f, 0.0f };
    };

    class AE_ASSET_API FMeshAsset final : public IAsset {
    public:
        FMeshAsset(FMeshRuntimeDesc desc, TVector<FMeshVertexAttributeDesc> attributes,
            TVector<FMeshSubMeshDesc> subMeshes, TVector<u8> vertexData, TVector<u8> indexData);

        [[nodiscard]] auto GetDesc() const noexcept -> const FMeshRuntimeDesc& { return mDesc; }
        [[nodiscard]] auto GetAttributes() const noexcept
            -> const TVector<FMeshVertexAttributeDesc>& {
            return mAttributes;
        }
        [[nodiscard]] auto GetSubMeshes() const noexcept -> const TVector<FMeshSubMeshDesc>& {
            return mSubMeshes;
        }
        [[nodiscard]] auto GetVertexData() const noexcept -> const TVector<u8>& {
            return mVertexData;
        }
        [[nodiscard]] auto GetIndexData() const noexcept -> const TVector<u8>& {
            return mIndexData;
        }

    private:
        FMeshRuntimeDesc                  mDesc{};
        TVector<FMeshVertexAttributeDesc> mAttributes;
        TVector<FMeshSubMeshDesc>         mSubMeshes;
        TVector<u8>                       mVertexData;
        TVector<u8>                       mIndexData;
    };

} // namespace AltinaEngine::Asset
