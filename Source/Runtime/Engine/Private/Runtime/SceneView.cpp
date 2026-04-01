#include "Engine/Runtime/SceneView.h"

#include "Engine/GameScene/CameraComponent.h"
#include "Engine/GameScene/DirectionalLightComponent.h"
#include "Engine/GameScene/MeshMaterialComponent.h"
#include "Engine/GameScene/PbrSkyComponent.h"
#include "Engine/GameScene/PointLightComponent.h"
#include "Engine/GameScene/SkyCubeComponent.h"
#include "Engine/GameScene/StaticMeshFilterComponent.h"
#include "Engine/GameScene/World.h"
#include "Utility/Assert.h"
#include "Math/LinAlg/Common.h"

using AltinaEngine::Move;
namespace AltinaEngine::Engine {
    namespace {
        auto BuildPbrSkyParameters(const GameScene::FPbrSkyComponent& component)
            -> FPbrSkySceneParameters {
            FPbrSkySceneParameters out{};
            const auto&            parameters = component.GetParameters();
            out.RayleighScattering            = parameters.mRayleighScattering;
            out.RayleighScaleHeightKm         = parameters.mRayleighScaleHeightKm;
            out.MieScattering                 = parameters.mMieScattering;
            out.MieAbsorption                 = parameters.mMieAbsorption;
            out.MieScaleHeightKm              = parameters.mMieScaleHeightKm;
            out.MieAnisotropy                 = parameters.mMieAnisotropy;
            out.OzoneAbsorption               = parameters.mOzoneAbsorption;
            out.OzoneCenterHeightKm           = parameters.mOzoneCenterHeightKm;
            out.OzoneThicknessKm              = parameters.mOzoneThicknessKm;
            out.GroundAlbedo                  = parameters.mGroundAlbedo;
            out.SolarTint                     = parameters.mSolarTint;
            out.SolarIlluminance              = parameters.mSolarIlluminance;
            out.SunAngularRadius              = parameters.mSunAngularRadius;
            out.PlanetRadiusKm                = parameters.mPlanetRadiusKm;
            out.AtmosphereHeightKm            = parameters.mAtmosphereHeightKm;
            out.ViewHeightKm                  = parameters.mViewHeightKm;
            out.Exposure                      = parameters.mExposure;
            out.Version                       = component.GetVersion();
            return out;
        }
    } // namespace

    void FSceneViewBuilder::Build(const GameScene::FWorld& world,
        const FSceneViewBuildParams& params, FRenderScene& outScene) const {
        outScene.Views.Clear();
        outScene.StaticMeshes.Clear();
        outScene.Lights.Clear();
        outScene.SkyProvider  = ESkyProviderType::None;
        outScene.SkyCubeAsset = {};
        outScene.bHasSkyCube  = false;
        outScene.PbrSky       = {};
        outScene.bHasPbrSky   = false;

        const auto appendView = [&](const RenderCore::View::FCameraData& cameraData,
                                    GameScene::FComponentId              cameraId) {
            FSceneView sceneView{};
            sceneView.CameraId = cameraId;
            sceneView.Target   = params.ViewTarget;

            auto& viewData               = sceneView.View;
            viewData.Camera              = cameraData;
            viewData.ViewRect            = params.ViewRect;
            viewData.RenderTargetExtent  = params.RenderTargetExtent;
            viewData.FrameIndex          = params.FrameIndex;
            viewData.TemporalSampleIndex = params.TemporalSampleIndex;
            viewData.DeltaTimeSeconds    = params.DeltaTimeSeconds;
            viewData.bReverseZ           = params.bReverseZ;
            viewData.BeginFrame();

            outScene.Views.PushBack(Move(sceneView));
        };

        if (params.PrimaryCameraOverride != nullptr) {
            appendView(*params.PrimaryCameraOverride, {});
        } else {
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

                RenderCore::View::FCameraData cameraData{};
                cameraData.mProjectionType = RenderCore::View::ECameraProjectionType::Perspective;
                cameraData.mVerticalFovRadians = component.GetFovYRadians();
                cameraData.mNearPlane          = component.GetNearPlane();
                cameraData.mFarPlane           = component.GetFarPlane();
                cameraData.mTransform = world.Object(component.GetOwner()).GetWorldTransform();
                appendView(cameraData, id);
            }
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
            entry.MeshGeometryKey     = meshComponent.GetStaticMeshGeometryKey();
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

            if (outScene.Lights.mHasMainDirectionalLight) {
                // Phase1: first enabled directional is treated as the main light.
                continue;
            }

            const auto transform = world.Object(component.GetOwner()).GetWorldTransform();
            const auto forward =
                transform.Rotation.RotateVector(Core::Math::FVector3f(0.0f, 0.0f, 1.0f));

            RenderCore::Lighting::FDirectionalLight light{};
            light.mDirectionWS        = forward;
            light.mColor              = component.mColor;
            light.mIntensity          = component.mIntensity;
            light.mCastShadows        = component.mCastShadows;
            light.mShadowCascadeCount = component.mShadowCascadeCount;
            light.mShadowSplitLambda  = component.mShadowSplitLambda;
            light.mShadowMaxDistance  = component.mShadowMaxDistance;
            light.mShadowMapSize      = component.mShadowMapSize;
            light.mShadowReceiverBias = component.mShadowReceiverBias;

            outScene.Lights.mHasMainDirectionalLight = true;
            outScene.Lights.mMainDirectionalLight    = light;
        }

        const auto& pointLightIds = world.GetActivePointLightComponents();
        outScene.Lights.mPointLights.Reserve(pointLightIds.Size());
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
            light.mPositionWS = transform.Translation;
            light.mRange      = component.mRange;
            light.mColor      = component.mColor;
            light.mIntensity  = component.mIntensity;
            outScene.Lights.mPointLights.PushBack(light);
        }

        if (!outScene.Lights.mHasMainDirectionalLight) {
            // Default main light so scenes are visible without explicit light authoring.
            outScene.Lights.mHasMainDirectionalLight = true;
            outScene.Lights.mMainDirectionalLight.mDirectionWS =
                Core::Math::FVector3f(0.4f, 0.6f, 0.7f);
            outScene.Lights.mMainDirectionalLight.mColor = Core::Math::FVector3f(1.0f, 1.0f, 1.0f);
            outScene.Lights.mMainDirectionalLight.mIntensity          = 2.0f;
            outScene.Lights.mMainDirectionalLight.mCastShadows        = false;
            outScene.Lights.mMainDirectionalLight.mShadowCascadeCount = 4U;
            outScene.Lights.mMainDirectionalLight.mShadowSplitLambda  = 0.65f;
            outScene.Lights.mMainDirectionalLight.mShadowMaxDistance  = 250.0f;
            outScene.Lights.mMainDirectionalLight.mShadowMapSize      = 2048U;
            outScene.Lights.mMainDirectionalLight.mShadowReceiverBias = 0.0015f;
        }

        u32         activeSkyProviderCount = 0U;
        const auto& skyCubeIds             = world.GetActiveSkyCubeComponents();
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

            ++activeSkyProviderCount;
            outScene.SkyProvider  = ESkyProviderType::SkyCube;
            outScene.SkyCubeAsset = handle;
            outScene.bHasSkyCube  = true;
        }

        const auto& pbrSkyIds = world.GetActivePbrSkyComponents();
        for (const auto& id : pbrSkyIds) {
            if (!world.IsAlive(id)) {
                continue;
            }

            const auto& component = world.ResolveComponent<GameScene::FPbrSkyComponent>(id);
            if (!component.IsEnabled() || !world.IsGameObjectActive(component.GetOwner())) {
                continue;
            }

            ++activeSkyProviderCount;
            outScene.SkyProvider = ESkyProviderType::PbrSky;
            outScene.PbrSky      = BuildPbrSkyParameters(component);
            outScene.bHasPbrSky  = true;
        }

        Core::Utility::Assert(activeSkyProviderCount <= 1U, TEXT("Engine.SceneView"),
            "Only one active sky provider is allowed. activeCount={}", activeSkyProviderCount);
    }
} // namespace AltinaEngine::Engine
