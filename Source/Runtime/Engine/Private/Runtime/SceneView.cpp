#include "Engine/Runtime/SceneView.h"

#include "Engine/GameScene/CameraComponent.h"
#include "Engine/GameScene/DirectionalLightComponent.h"
#include "Engine/GameScene/MeshMaterialComponent.h"
#include "Engine/GameScene/PointLightComponent.h"
#include "Engine/GameScene/SkyCubeComponent.h"
#include "Engine/GameScene/StaticMeshFilterComponent.h"
#include "Engine/GameScene/World.h"
#include "Math/LinAlg/Common.h"

using AltinaEngine::Move;
namespace AltinaEngine::Engine {
    void FSceneViewBuilder::Build(const GameScene::FWorld& world,
        const FSceneViewBuildParams& params, FRenderScene& outScene) const {
        outScene.Views.Clear();
        outScene.StaticMeshes.Clear();
        outScene.Lights.Clear();
        outScene.SkyCubeAsset = {};
        outScene.bHasSkyCube  = false;

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

            auto& viewData                  = sceneView.View;
            viewData.Camera.mProjectionType = RenderCore::View::ECameraProjectionType::Perspective;
            viewData.Camera.mVerticalFovRadians = component.GetFovYRadians();
            viewData.Camera.mNearPlane          = component.GetNearPlane();
            viewData.Camera.mFarPlane           = component.GetFarPlane();
            viewData.Camera.mTransform = world.Object(component.GetOwner()).GetWorldTransform();

            viewData.ViewRect            = params.ViewRect;
            viewData.RenderTargetExtent  = params.RenderTargetExtent;
            viewData.FrameIndex          = params.FrameIndex;
            viewData.TemporalSampleIndex = params.TemporalSampleIndex;
            viewData.DeltaTimeSeconds    = params.DeltaTimeSeconds;
            viewData.bReverseZ           = params.bReverseZ;

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

            const auto& meshData = meshComponent.GetStaticMesh();
            if (!meshData.IsValid()) {
                continue;
            }

            const auto       owner = materialComponent.GetOwner();
            FSceneStaticMesh entry{};
            entry.OwnerId             = owner;
            entry.MeshComponentId     = meshId;
            entry.MaterialComponentId = id;
            entry.Mesh                = &meshData;
            entry.Materials           = &materialComponent;
            const auto worldTransform = world.Object(owner).GetWorldTransform();
            entry.WorldMatrix         = worldTransform.ToMatrix();
            entry.PrevWorldMatrix     = entry.WorldMatrix;
            outScene.StaticMeshes.PushBack(entry);
        }

        // Lights (Phase1: single main directional + N points).
        const auto& dirLightIds = world.GetActiveDirectionalLightComponents();
        for (const auto& id : dirLightIds) {
            if (!world.IsAlive(id)) {
                continue;
            }

            const auto& component =
                world.ResolveComponent<GameScene::FDirectionalLightComponent>(id);
            if (!component.IsEnabled() || !world.IsGameObjectActive(component.GetOwner())) {
                continue;
            }

            if (outScene.Lights.bHasMainDirectionalLight) {
                // Phase1: first enabled directional is treated as the main light.
                continue;
            }

            const auto transform = world.Object(component.GetOwner()).GetWorldTransform();
            const auto forward =
                transform.Rotation.RotateVector(Core::Math::FVector3f(0.0f, 0.0f, 1.0f));

            RenderCore::Lighting::FDirectionalLight light{};
            light.DirectionWS        = forward;
            light.Color              = component.mColor;
            light.Intensity          = component.mIntensity;
            light.bCastShadows       = component.mCastShadows;
            light.ShadowCascadeCount = component.mShadowCascadeCount;
            light.ShadowSplitLambda  = component.mShadowSplitLambda;
            light.ShadowMaxDistance  = component.mShadowMaxDistance;
            light.ShadowMapSize      = component.mShadowMapSize;
            light.ShadowReceiverBias = component.mShadowReceiverBias;

            outScene.Lights.bHasMainDirectionalLight = true;
            outScene.Lights.MainDirectionalLight     = light;
        }

        const auto& pointLightIds = world.GetActivePointLightComponents();
        outScene.Lights.PointLights.Reserve(pointLightIds.Size());
        for (const auto& id : pointLightIds) {
            if (!world.IsAlive(id)) {
                continue;
            }

            const auto& component = world.ResolveComponent<GameScene::FPointLightComponent>(id);
            if (!component.IsEnabled() || !world.IsGameObjectActive(component.GetOwner())) {
                continue;
            }

            const auto transform = world.Object(component.GetOwner()).GetWorldTransform();

            RenderCore::Lighting::FPointLight light{};
            light.PositionWS = transform.Translation;
            light.Range      = component.mRange;
            light.Color      = component.mColor;
            light.Intensity  = component.mIntensity;
            outScene.Lights.PointLights.PushBack(light);
        }

        if (!outScene.Lights.bHasMainDirectionalLight) {
            // Default main light so scenes are visible without explicit light authoring.
            outScene.Lights.bHasMainDirectionalLight = true;
            outScene.Lights.MainDirectionalLight.DirectionWS =
                Core::Math::FVector3f(0.4f, 0.6f, 0.7f);
            outScene.Lights.MainDirectionalLight.Color = Core::Math::FVector3f(1.0f, 1.0f, 1.0f);
            outScene.Lights.MainDirectionalLight.Intensity          = 2.0f;
            outScene.Lights.MainDirectionalLight.bCastShadows       = false;
            outScene.Lights.MainDirectionalLight.ShadowCascadeCount = 4U;
            outScene.Lights.MainDirectionalLight.ShadowSplitLambda  = 0.65f;
            outScene.Lights.MainDirectionalLight.ShadowMaxDistance  = 250.0f;
            outScene.Lights.MainDirectionalLight.ShadowMapSize      = 2048U;
            outScene.Lights.MainDirectionalLight.ShadowReceiverBias = 0.0015f;
        }

        // Sky cube: first enabled instance with a valid asset handle wins.
        const auto& skyCubeIds = world.GetActiveSkyCubeComponents();
        for (const auto& id : skyCubeIds) {
            if (!world.IsAlive(id)) {
                continue;
            }

            const auto& component = world.ResolveComponent<GameScene::FSkyCubeComponent>(id);
            if (!component.IsEnabled() || !world.IsGameObjectActive(component.GetOwner())) {
                continue;
            }

            const auto handle = component.GetCubeMapAsset();
            if (!handle.IsValid()) {
                continue;
            }

            outScene.SkyCubeAsset = handle;
            outScene.bHasSkyCube  = true;
            break;
        }
    }
} // namespace AltinaEngine::Engine
