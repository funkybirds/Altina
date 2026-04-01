#include "Engine/Runtime/StaticMeshCache.h"

namespace AltinaEngine::Engine {
    namespace {
        void InitStaticMeshGpuResources(Geometry::FStaticMeshData& mesh) {
            for (auto& lod : mesh.mLods) {
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

        void ReleaseStaticMeshGpuResources(Geometry::FStaticMeshData& mesh) noexcept {
            for (auto& lod : mesh.mLods) {
                lod.ReleaseGpuResources();
            }
        }
    } // namespace

    auto FStaticMeshCache::ResolveMesh(const Asset::FAssetHandle& handle, FMeshBuilder builder)
        -> Container::TShared<FStaticMeshCacheEntry> {
        if (!handle.IsValid() || !builder) {
            return {};
        }

        const auto it = mMeshCache.FindIt(handle);
        if (it != mMeshCache.end()) {
            return it->second;
        }

        Geometry::FStaticMeshData mesh{};
        if (!builder(handle, mesh)) {
            return {};
        }

        InitStaticMeshGpuResources(mesh);

        auto sharedEntry          = Container::MakeShared<FStaticMeshCacheEntry>();
        sharedEntry->mHandle      = handle;
        sharedEntry->mGeometryKey = BuildGeometryKey(handle);
        sharedEntry->mMesh        = Move(mesh);

        mMeshCache.Emplace(handle, sharedEntry);
        return sharedEntry;
    }

    void FStaticMeshCache::Clear() {
        for (auto& entry : mMeshCache) {
            if (!entry.second) {
                continue;
            }
            ReleaseStaticMeshGpuResources(entry.second->mMesh);
        }
        mMeshCache.Clear();
    }

    auto FStaticMeshCache::FStaticMeshCacheKeyHash::operator()(
        const Asset::FAssetHandle& handle) const noexcept -> usize {
        return static_cast<usize>(BuildGeometryKey(handle));
    }

    auto FStaticMeshCache::BuildGeometryKey(const Asset::FAssetHandle& handle) noexcept -> u64 {
        constexpr u64 kFnvOffset64 = 1469598103934665603ULL;
        constexpr u64 kFnvPrime64  = 1099511628211ULL;

        u64           hash  = kFnvOffset64;
        const auto*   bytes = handle.mUuid.Data();
        for (usize i = 0U; i < FUuid::kByteCount; ++i) {
            hash ^= bytes[i];
            hash *= kFnvPrime64;
        }
        hash ^= static_cast<u8>(handle.mType);
        hash *= kFnvPrime64;
        return hash;
    }
} // namespace AltinaEngine::Engine
