#include "Engine/Runtime/SceneView.h"

#include "Engine/GameScene/CameraComponent.h"
#include "Engine/GameScene/MeshMaterialComponent.h"
#include "Engine/GameScene/StaticMeshFilterComponent.h"
#include "Engine/GameScene/World.h"

using AltinaEngine::Move;
namespace AltinaEngine::Engine {
    void FSceneViewBuilder::Build(const GameScene::FWorld& world,
        const FSceneViewBuildParams& params, FRenderScene& outScene) const {
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
            sceneView.Target   = params.ViewTarget;

            auto& viewData                 = sceneView.View;
            viewData.Camera.ProjectionType = RenderCore::View::ECameraProjectionType::Perspective;
            viewData.Camera.VerticalFovRadians = component.GetFovYRadians();
            viewData.Camera.NearPlane          = component.GetNearPlane();
            viewData.Camera.FarPlane           = component.GetFarPlane();

            viewData.ViewRect            = params.ViewRect;
            viewData.RenderTargetExtent  = params.RenderTargetExtent;
            viewData.FrameIndex          = params.FrameIndex;
            viewData.TemporalSampleIndex = params.TemporalSampleIndex;
            viewData.DeltaTimeSeconds    = params.DeltaTimeSeconds;
            viewData.bReverseZ           = params.bReverseZ;

            viewData.BeginFrame();

            outScene.Views.PushBack(Move(sceneView));
        }

        const auto& meshMaterialIds = world.GetActiveMeshMaterialComponents();
        outScene.StaticMeshes.Reserve(meshMaterialIds.Size());
        for (const auto& id : meshMaterialIds) {
            if (!world.IsAlive(id)) {
                continue;
            }

            const auto& materialComponent =
                world.ResolveComponent<GameScene::FMeshMaterialComponent>(id);
            if (!materialComponent.IsEnabled()
                || !world.IsGameObjectActive(materialComponent.GetOwner())) {
                continue;
            }

            const auto meshId = world.GetComponent<GameScene::FStaticMeshFilterComponent>(
                materialComponent.GetOwner());
            if (!meshId.IsValid()) {
                continue;
            }

            const auto& meshComponent =
                world.ResolveComponent<GameScene::FStaticMeshFilterComponent>(meshId);
            if (!meshComponent.IsEnabled()) {
                continue;
            }

            const auto       owner = materialComponent.GetOwner();
            FSceneStaticMesh entry{};
            entry.OwnerId             = owner;
            entry.MeshComponentId     = meshId;
            entry.MaterialComponentId = id;
            entry.Mesh                = &meshComponent.GetStaticMesh();
            entry.Materials           = &materialComponent;
            const auto worldTransform = world.Object(owner).GetWorldTransform();
            entry.WorldMatrix         = worldTransform.ToMatrix();
            entry.PrevWorldMatrix     = entry.WorldMatrix;
            outScene.StaticMeshes.PushBack(entry);
        }
    }
} // namespace AltinaEngine::Engine
