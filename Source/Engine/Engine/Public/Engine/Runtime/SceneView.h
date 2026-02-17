#pragma once

#include "Engine/EngineAPI.h"
#include "Engine/GameScene/Ids.h"
#include "Container/Vector.h"
#include "Types/Aliases.h"
#include "View/ViewData.h"

namespace AltinaEngine::GameScene {
    class FWorld;
    class FMeshMaterialComponent;
}

namespace AltinaEngine::RenderCore::Geometry {
    struct FStaticMeshData;
}

namespace AltinaEngine::Engine {
    namespace Container = Core::Container;
    using Container::TVector;

    struct AE_ENGINE_API FSceneView {
        GameScene::FComponentId  CameraId{};
        RenderCore::View::FViewData View{};
    };

    struct AE_ENGINE_API FSceneStaticMesh {
        GameScene::FComponentId                     MeshComponentId{};
        GameScene::FComponentId                     MaterialComponentId{};
        const RenderCore::Geometry::FStaticMeshData* Mesh = nullptr;
        const GameScene::FMeshMaterialComponent*     Materials = nullptr;
    };

    struct AE_ENGINE_API FRenderScene {
        TVector<FSceneView>       Views{};
        TVector<FSceneStaticMesh> StaticMeshes{};
    };

    struct AE_ENGINE_API FSceneViewBuildParams {
        RenderCore::View::FViewRect             ViewRect{};
        RenderCore::View::FRenderTargetExtent2D RenderTargetExtent{};
        u64                                     FrameIndex          = 0ULL;
        u32                                     TemporalSampleIndex = 0U;
        f32                                     DeltaTimeSeconds    = 0.0f;
        bool                                    bReverseZ           = true;
    };

    class AE_ENGINE_API FSceneViewBuilder {
    public:
        void Build(const GameScene::FWorld& world, const FSceneViewBuildParams& params,
            FRenderScene& outScene) const;
    };
} // namespace AltinaEngine::Engine
