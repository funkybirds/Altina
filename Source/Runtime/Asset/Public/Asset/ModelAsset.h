#pragma once

#include "Asset/AssetBinary.h"
#include "Asset/AssetLoader.h"
#include "Container/Vector.h"

namespace AltinaEngine::Asset {
    namespace Container = Core::Container;
    using Container::TVector;

    struct AE_ASSET_API FModelRuntimeDesc {
        u32 NodeCount         = 0;
        u32 MeshRefCount      = 0;
        u32 MaterialSlotCount = 0;
    };

    class AE_ASSET_API FModelAsset final : public IAsset {
    public:
        FModelAsset(FModelRuntimeDesc desc, TVector<FModelNodeDesc> nodes,
            TVector<FModelMeshRef> meshRefs, TVector<FAssetHandle> materialSlots);

        [[nodiscard]] auto GetDesc() const noexcept -> const FModelRuntimeDesc& { return mDesc; }
        [[nodiscard]] auto GetNodes() const noexcept -> const TVector<FModelNodeDesc>& {
            return mNodes;
        }
        [[nodiscard]] auto GetMeshRefs() const noexcept -> const TVector<FModelMeshRef>& {
            return mMeshRefs;
        }
        [[nodiscard]] auto GetMaterialSlots() const noexcept -> const TVector<FAssetHandle>& {
            return mMaterialSlots;
        }

    private:
        FModelRuntimeDesc       mDesc{};
        TVector<FModelNodeDesc> mNodes;
        TVector<FModelMeshRef>  mMeshRefs;
        TVector<FAssetHandle>   mMaterialSlots;
    };

} // namespace AltinaEngine::Asset
