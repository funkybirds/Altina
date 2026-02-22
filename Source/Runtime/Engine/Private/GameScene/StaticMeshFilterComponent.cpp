#include "Engine/GameScene/StaticMeshFilterComponent.h"

namespace AltinaEngine::GameScene {
    FStaticMeshFilterComponent::FAssetToStaticMeshConverter
         FStaticMeshFilterComponent::AssetToStaticMeshConverter = {};

    auto FStaticMeshFilterComponent::GetStaticMesh() noexcept -> Geometry::FStaticMeshData& {
        ResolveStaticMesh();
        return mStaticMesh;
    }

    auto FStaticMeshFilterComponent::GetStaticMesh() const noexcept
        -> const Geometry::FStaticMeshData& {
        ResolveStaticMesh();
        return mStaticMesh;
    }

    void FStaticMeshFilterComponent::SetStaticMeshAsset(Asset::FAssetHandle handle) noexcept {
        mMeshAsset     = Move(handle);
        mMeshResolved  = false;
        mStaticMesh    = {};
        mResolvedAsset = {};
    }

    void FStaticMeshFilterComponent::ResolveStaticMesh() const noexcept {
        if (mResolvedAsset != mMeshAsset) {
            mMeshResolved  = false;
            mResolvedAsset = mMeshAsset;
        }

        if (mMeshResolved) {
            return;
        }

        if (!AssetToStaticMeshConverter) {
            return;
        }

        mMeshResolved = true;
        mStaticMesh   = {};

        if (!mMeshAsset.IsValid()) {
            return;
        }

        mStaticMesh = AssetToStaticMeshConverter(mMeshAsset);
    }
} // namespace AltinaEngine::GameScene
