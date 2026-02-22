#pragma once

#include "Engine/EngineAPI.h"
#include "Engine/GameScene/Component.h"
#include "Asset/AssetTypes.h"
#include "Container/Function.h"
#include "Geometry/StaticMeshData.h"
#include "Types/Traits.h"
#include "Reflection/ReflectionAnnotations.h"
#include "Reflection/ReflectionFwd.h"

namespace AltinaEngine::GameScene {
    using AltinaEngine::Move;
    namespace Asset     = AltinaEngine::Asset;
    namespace Container = Core::Container;
    namespace Geometry  = RenderCore::Geometry;

    /**
     * @brief Component holding static mesh render data.
     */
    class ACLASS() AE_ENGINE_API FStaticMeshFilterComponent : public FComponent {
    public:
        using FAssetToStaticMeshConverter =
            Container::TFunction<Geometry::FStaticMeshData(const Asset::FAssetHandle&)>;

        [[nodiscard]] auto GetStaticMesh() noexcept -> Geometry::FStaticMeshData&;
        [[nodiscard]] auto GetStaticMesh() const noexcept -> const Geometry::FStaticMeshData&;

        void               SetStaticMeshAsset(Asset::FAssetHandle handle) noexcept;
        [[nodiscard]] auto GetStaticMeshAsset() const noexcept -> Asset::FAssetHandle {
            return mMeshAsset;
        }

        static FAssetToStaticMeshConverter AssetToStaticMeshConverter;

    private:
        template <auto Member>
        friend struct AltinaEngine::Core::Reflection::Detail::TAutoMemberAccessor;

        void ResolveStaticMesh() const noexcept;

    public:
        APROPERTY()
        Asset::FAssetHandle mMeshAsset{};

    private:
        mutable Geometry::FStaticMeshData mStaticMesh{};
        mutable Asset::FAssetHandle       mResolvedAsset{};
        mutable bool                      mMeshResolved = false;
    };
} // namespace AltinaEngine::GameScene
