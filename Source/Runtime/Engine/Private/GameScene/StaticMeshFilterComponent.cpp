#include "Engine/GameScene/StaticMeshFilterComponent.h"

#include <cstdint>

namespace AltinaEngine::GameScene {
    namespace {
        [[nodiscard]] auto BuildProceduralGeometryKey(
            const Geometry::FStaticMeshData& mesh) noexcept -> u64 {
            return static_cast<u64>(reinterpret_cast<uintptr_t>(&mesh));
        }

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
        return const_cast<Geometry::FStaticMeshData&>(GetResolvedStaticMesh());
    }

    auto FStaticMeshFilterComponent::GetStaticMesh() const noexcept
        -> const Geometry::FStaticMeshData& {
        ResolveStaticMesh();
        return GetResolvedStaticMesh();
    }

    auto FStaticMeshFilterComponent::GetStaticMeshGeometryKey() const noexcept -> u64 {
        ResolveStaticMesh();
        if (mProceduralOverride) {
            return BuildProceduralGeometryKey(mStaticMesh);
        }
        if (mStaticMeshEntry) {
            return mStaticMeshEntry->mGeometryKey;
        }
        return 0ULL;
    }

    void FStaticMeshFilterComponent::SetStaticMeshAsset(Asset::FAssetHandle handle) noexcept {
        ReleaseStaticMeshGpuResources(mStaticMesh);
        mMeshAsset          = Move(handle);
        mProceduralOverride = false;
        mMeshResolved       = false;
        mStaticMesh         = {};
        mStaticMeshEntry.Reset();
        mResolvedAsset = {};
    }

    void FStaticMeshFilterComponent::SetStaticMeshData(
        Geometry::FStaticMeshData&& InMesh) noexcept {
        ReleaseStaticMeshGpuResources(mStaticMesh);
        mProceduralOverride = true;
        mMeshAsset          = {};
        mResolvedAsset      = {};
        mMeshResolved       = true;
        mStaticMesh         = Move(InMesh);
        mStaticMeshEntry.Reset();

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
        mStaticMeshEntry.Reset();
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
        mStaticMeshEntry.Reset();

        if (!mMeshAsset.IsValid()) {
            return;
        }

        mStaticMeshEntry = AssetToStaticMeshConverter(mMeshAsset);
    }

    auto FStaticMeshFilterComponent::GetResolvedStaticMesh() const noexcept
        -> const Geometry::FStaticMeshData& {
        if (mProceduralOverride || !mStaticMeshEntry) {
            return mStaticMesh;
        }
        return mStaticMeshEntry->mMesh;
    }
} // namespace AltinaEngine::GameScene
