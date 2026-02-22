#pragma once

#include "Engine/EngineAPI.h"
#include "Engine/GameScene/Component.h"
#include "Geometry/StaticMeshData.h"
#include "Types/Traits.h"
#include "Reflection/ReflectionAnnotations.h"
#include "Reflection/ReflectionFwd.h"

namespace AltinaEngine::GameScene {
    namespace Geometry = RenderCore::Geometry;

    /**
     * @brief Component holding static mesh render data.
     */
    class ACLASS() AE_ENGINE_API FStaticMeshFilterComponent : public FComponent {
    public:
        [[nodiscard]] auto GetStaticMesh() noexcept -> Geometry::FStaticMeshData& {
            return mStaticMesh;
        }
        [[nodiscard]] auto GetStaticMesh() const noexcept -> const Geometry::FStaticMeshData& {
            return mStaticMesh;
        }
        void SetStaticMesh(Geometry::FStaticMeshData&& mesh) noexcept { mStaticMesh = Move(mesh); }

    private:
        template <auto Member>
        friend struct AltinaEngine::Core::Reflection::Detail::TAutoMemberAccessor;

    public:
        APROPERTY()
        Geometry::FStaticMeshData mStaticMesh{};
    };
} // namespace AltinaEngine::GameScene
