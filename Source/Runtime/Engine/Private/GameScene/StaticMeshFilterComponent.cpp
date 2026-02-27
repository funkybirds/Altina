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
        mMeshAsset          = Move(handle);
        mProceduralOverride = false;
        mMeshResolved       = false;
        mStaticMesh         = {};
        mResolvedAsset      = {};
    }

    void FStaticMeshFilterComponent::SetStaticMeshData(
        Geometry::FStaticMeshData&& InMesh) noexcept {
        mProceduralOverride = true;
        mMeshAsset          = {};
        mResolvedAsset      = {};
        mMeshResolved       = true;
        mStaticMesh         = Move(InMesh);

        // Mirror the mesh-asset conversion path: ensure GPU buffers exist so the procedural mesh is
        // drawable immediately.
        for (auto& lod : mStaticMesh.Lods) {
            lod.PositionBuffer.InitResource();
            lod.IndexBuffer.InitResource();
            lod.TangentBuffer.InitResource();
            lod.UV0Buffer.InitResource();
            lod.UV1Buffer.InitResource();

            lod.PositionBuffer.WaitForInit();
            lod.IndexBuffer.WaitForInit();
            lod.TangentBuffer.WaitForInit();
            lod.UV0Buffer.WaitForInit();
            lod.UV1Buffer.WaitForInit();
        }
    }

    void FStaticMeshFilterComponent::ClearStaticMeshData() noexcept {
        mProceduralOverride = false;
        mMeshAsset          = {};
        mResolvedAsset      = {};
        mMeshResolved       = false;
        mStaticMesh         = {};
    }

    void FStaticMeshFilterComponent::ResolveStaticMesh() const noexcept {
        if (mProceduralOverride) {
            return;
        }

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
