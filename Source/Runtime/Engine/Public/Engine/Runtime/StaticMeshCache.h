#pragma once

#include "Engine/EngineAPI.h"

#include "Asset/AssetTypes.h"
#include "Container/Function.h"
#include "Container/HashMap.h"
#include "Container/SmartPtr.h"
#include "Geometry/StaticMeshData.h"
#include "Types/Aliases.h"

namespace AltinaEngine::Engine {
    namespace Asset     = AltinaEngine::Asset;
    namespace Container = Core::Container;
    namespace Geometry  = RenderCore::Geometry;

    struct AE_ENGINE_API FStaticMeshCacheEntry {
        Asset::FAssetHandle       mHandle{};
        u64                       mGeometryKey = 0ULL;
        Geometry::FStaticMeshData mMesh{};
    };

    class AE_ENGINE_API FStaticMeshCache {
    public:
        using FMeshBuilder =
            Container::TFunction<bool(const Asset::FAssetHandle&, Geometry::FStaticMeshData&)>;

        [[nodiscard]] auto ResolveMesh(const Asset::FAssetHandle& handle, FMeshBuilder builder)
            -> Container::TShared<FStaticMeshCacheEntry>;

        void Clear();

    private:
        struct FStaticMeshCacheKeyHash {
            auto operator()(const Asset::FAssetHandle& handle) const noexcept -> usize;
        };

        [[nodiscard]] static auto BuildGeometryKey(const Asset::FAssetHandle& handle) noexcept
            -> u64;

        Container::THashMap<Asset::FAssetHandle, Container::TShared<FStaticMeshCacheEntry>,
            FStaticMeshCacheKeyHash>
            mMeshCache;
    };
} // namespace AltinaEngine::Engine
