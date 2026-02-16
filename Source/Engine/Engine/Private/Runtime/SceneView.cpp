#include "Engine/Runtime/SceneView.h"

#include "Engine/GameScene/CameraComponent.h"
#include "Engine/GameScene/StaticMeshFilterComponent.h"
#include "Engine/GameScene/World.h"

using AltinaEngine::Move;
namespace AltinaEngine::Engine {
    void FSceneViewBuilder::Build(
        const GameScene::FWorld& world, const FSceneViewBuildParams& params,
        FRenderScene& outScene) const {
        outScene.Views.Clear();
        outScene.StaticMeshes.Clear();

        const auto& cameraIds = world.GetActiveCameraComponents();
        outScene.Views.Reserve(cameraIds.Size());
        for (const auto& id : cameraIds) {
            if (!world.IsAlive(id)) {
                continue;
            }

            const auto& component = world.ResolveComponent<GameScene::FCameraComponent>(id);
            if (!component.IsEnabled() || !world.IsGameObjectActive(component.GetOwner())) {
                continue;
            }

            FSceneView sceneView{};
            sceneView.CameraId = id;

            auto& viewData              = sceneView.View;
            viewData.Camera.ProjectionType =
                RenderCore::View::ECameraProjectionType::Perspective;
            viewData.Camera.VerticalFovRadians = component.GetFovYRadians();
            viewData.Camera.NearPlane          = component.GetNearPlane();
            viewData.Camera.FarPlane           = component.GetFarPlane();

            viewData.ViewRect             = params.ViewRect;
            viewData.RenderTargetExtent   = params.RenderTargetExtent;
            viewData.FrameIndex           = params.FrameIndex;
            viewData.TemporalSampleIndex  = params.TemporalSampleIndex;
            viewData.DeltaTimeSeconds     = params.DeltaTimeSeconds;
            viewData.bReverseZ            = params.bReverseZ;

            viewData.BeginFrame();

            outScene.Views.PushBack(Move(sceneView));
        }

        const auto& staticMeshIds = world.GetActiveStaticMeshComponents();
        outScene.StaticMeshes.Reserve(staticMeshIds.Size());
        for (const auto& id : staticMeshIds) {
            if (!world.IsAlive(id)) {
                continue;
            }

            const auto& component =
                world.ResolveComponent<GameScene::FStaticMeshFilterComponent>(id);
            if (!component.IsEnabled() || !world.IsGameObjectActive(component.GetOwner())) {
                continue;
            }

            FSceneStaticMesh entry{};
            entry.ComponentId = id;
            entry.Mesh        = &component.GetStaticMesh();
            outScene.StaticMeshes.PushBack(entry);
        }
    }
} // namespace AltinaEngine::Engine
