#include "Engine/GameScene/StaticMeshFilterComponent.h"

namespace AltinaEngine::GameScene {
    namespace {
        void ReleaseStaticMeshGpuResources(Geometry::FStaticMeshData& mesh) noexcept {
            for (auto& lod : mesh.mLods) {
                lod.ReleaseGpuResources();
            }
        }
    } // namespace

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
        ReleaseStaticMeshGpuResources(mStaticMesh);
        mMeshAsset          = Move(handle);
        mProceduralOverride = false;
        mMeshResolved       = false;
        mStaticMesh         = {};
        mResolvedAsset      = {};
    }

    void FStaticMeshFilterComponent::SetStaticMeshData(
        Geometry::FStaticMeshData&& InMesh) noexcept {
        ReleaseStaticMeshGpuResources(mStaticMesh);
        mProceduralOverride = true;
        mMeshAsset          = {};
        mResolvedAsset      = {};
        mMeshResolved       = true;
        mStaticMesh         = Move(InMesh);

        // Mirror the mesh-asset conversion path: ensure GPU buffers exist so the procedural mesh is
        // drawable immediately.
        for (auto& lod : mStaticMesh.mLods) {
            lod.mPositionBuffer.InitResource();
            lod.mIndexBuffer.InitResource();
            lod.mTangentBuffer.InitResource();
            lod.mUV0Buffer.InitResource();
            lod.mUV1Buffer.InitResource();

            lod.mPositionBuffer.WaitForInit();
            lod.mIndexBuffer.WaitForInit();
            lod.mTangentBuffer.WaitForInit();
            lod.mUV0Buffer.WaitForInit();
            lod.mUV1Buffer.WaitForInit();
        }
    }

    void FStaticMeshFilterComponent::ClearStaticMeshData() noexcept {
        ReleaseStaticMeshGpuResources(mStaticMesh);
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
        ReleaseStaticMeshGpuResources(mStaticMesh);
        mStaticMesh = {};

        if (!mMeshAsset.IsValid()) {
            return;
        }

        mStaticMesh = AssetToStaticMeshConverter(mMeshAsset);
    }
} // namespace AltinaEngine::GameScene
