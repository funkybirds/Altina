#pragma once

#include "Engine/EngineAPI.h"
#include "Engine/GameScene/Component.h"
#include "Engine/Runtime/StaticMeshCache.h"
#include "Asset/AssetTypes.h"
#include "Container/Function.h"
#include "Container/SmartPtr.h"
#include "Geometry/StaticMeshData.h"
#include "Types/Traits.h"
#include "Reflection/ReflectionAnnotations.h"
#include "Reflection/ReflectionFwd.h"

namespace AltinaEngine::GameScene {
    using AltinaEngine::Move;
    namespace Asset     = AltinaEngine::Asset;
    namespace Container = Core::Container;
    namespace Geometry  = RenderCore::Geometry;
    namespace Engine    = AltinaEngine::Engine;

    /**
     * @brief Component holding static mesh render data.
     */
    class ACLASS() AE_ENGINE_API FStaticMeshFilterComponent : public FComponent {
    public:
        using FAssetToStaticMeshConverter =
            Container::TFunction<Container::TShared<Engine::FStaticMeshCacheEntry>(
                const Asset::FAssetHandle&)>;

        [[nodiscard]] auto GetStaticMesh() noexcept -> Geometry::FStaticMeshData&;
        [[nodiscard]] auto GetStaticMesh() const noexcept -> const Geometry::FStaticMeshData&;
        [[nodiscard]] auto GetStaticMeshGeometryKey() const noexcept -> u64;

        void               SetStaticMeshAsset(Asset::FAssetHandle handle) noexcept;
        void               SetStaticMeshData(Geometry::FStaticMeshData&& InMesh) noexcept;
        void               ClearStaticMeshData() noexcept;
        [[nodiscard]] auto GetStaticMeshAsset() const noexcept -> Asset::FAssetHandle {
            return mMeshAsset;
        }

        static FAssetToStaticMeshConverter AssetToStaticMeshConverter;

    private:
        [[nodiscard]] auto GetResolvedStaticMesh() const noexcept
            -> const Geometry::FStaticMeshData&;
        template <auto Member>
        friend struct AltinaEngine::Core::Reflection::Detail::TAutoMemberAccessor;

        void ResolveStaticMesh() const noexcept;

    public:
        APROPERTY()
        Asset::FAssetHandle mMeshAsset{};

    private:
        mutable Geometry::FStaticMeshData                         mStaticMesh{};
        mutable Container::TShared<Engine::FStaticMeshCacheEntry> mStaticMeshEntry{};
        mutable Asset::FAssetHandle                               mResolvedAsset{};
        mutable bool                                              mMeshResolved       = false;
        mutable bool                                              mProceduralOverride = false;
    };
} // namespace AltinaEngine::GameScene
