#pragma once

#include "Engine/EngineAPI.h"
#include "Engine/GameScene/Component.h"
#include "Geometry/StaticMeshData.h"

namespace AltinaEngine::GameScene {
    namespace Geometry = RenderCore::Geometry;

    /**
     * @brief Component holding static mesh render data.
     */
    class AE_ENGINE_API FStaticMeshFilterComponent : public FComponent {
    public:
        [[nodiscard]] auto GetStaticMesh() noexcept -> Geometry::FStaticMeshData& {
            return mStaticMesh;
        }
        [[nodiscard]] auto GetStaticMesh() const noexcept -> const Geometry::FStaticMeshData& {
            return mStaticMesh;
        }
        void               SetStaticMesh(const Geometry::FStaticMeshData& mesh) {
            mStaticMesh = mesh;
        }

    private:
        Geometry::FStaticMeshData mStaticMesh{};
    };
} // namespace AltinaEngine::GameScene
