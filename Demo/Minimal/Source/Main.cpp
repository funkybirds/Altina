#include "Base/AltinaBase.h"
#include "Asset/AssetBinary.h"
#include "Asset/AssetManager.h"
#include "Asset/AssetRegistry.h"
#include "Engine/GameScene/CameraComponent.h"
#include "Engine/GameScene/DirectionalLightComponent.h"
#include "Engine/GameScene/MeshMaterialComponent.h"
#include "Engine/GameScene/ScriptComponent.h"
#include "Engine/GameScene/SkyCubeComponent.h"
#include "Engine/GameScene/StaticMeshFilterComponent.h"
#include "Engine/GameScene/World.h"
#include "Engine/GameSceneAsset/ModelAssetInstantiator.h"
#include "Launch/EngineLoop.h"
#include "Launch/GameClient.h"
#include "Logging/Log.h"
#include "Material/MaterialPass.h"
#include "Container/SmartPtr.h"
#include "Math/Common.h"
#include "Math/LinAlg/SpatialTransform.h"
#include "Math/Rotation.h"
#include "Math/Vector.h"
#include "Platform/Generic/GenericPlatformDecl.h"
#include "Platform/PlatformFileSystem.h"
#include "Rendering/BasicDeferredRenderer.h"
#include "Rendering/PostProcess/PostProcessSettings.h"
#include "Reflection/JsonSerializer.h"
#include "Rhi/RhiInit.h"
#include "Types/Aliases.h"
#include "Types/Traits.h"

using namespace AltinaEngine;

namespace {
    namespace Container = Core::Container;
    using Container::FStringView;

} // namespace

namespace {
    class FMinimalGameClient final : public Launch::FGameClient {
    public:
        auto OnInit(Launch::FEngineLoop& engineLoop) -> bool override {
            auto& assetManager = engineLoop.GetAssetManager();

            // Enable FXAA in the Minimal demo so RenderDoc captures show the pass by default.
            Rendering::rPostProcessFxaa.Set(1);

            const auto modelHandle = engineLoop.GetAssetRegistry().FindByPath(
                TEXT("demo/minimal/models/hoshino/hoshino_battle"));
            const auto scriptHandle =
                engineLoop.GetAssetRegistry().FindByPath(TEXT("demo/minimal/scripts/demoscript"));
            const auto skyCubeHandle = engineLoop.GetAssetRegistry().FindByPath(
                TEXT("demo/minimal/skyboxes/churchmeetingroom4k"));
            if (!modelHandle.IsValid() || !scriptHandle.IsValid()) {
                LogError(TEXT("Demo assets missing (model or script)."));
                return false;
            }

            auto&      worldManager = engineLoop.GetWorldManager();
            const auto worldHandle  = worldManager.CreateWorld();
            worldManager.SetActiveWorld(worldHandle);
            auto* world = worldManager.GetWorld(worldHandle);
            if (world == nullptr) {
                LogError(TEXT("Demo world creation failed."));
                return false;
            }

            auto cameraObject    = world->CreateGameObject(TEXT("Camera"));
            auto cameraComponent = cameraObject.AddComponent<GameScene::FCameraComponent>();
            auto scriptComponent = cameraObject.AddComponent<GameScene::FScriptComponent>();
            Core::Math::FQuaternion cameraRotationWS = Core::Math::FQuaternion::Identity();
            if (cameraComponent.IsValid()) {
                auto& camera = cameraComponent.Get();
                camera.SetNearPlane(0.1f);
                camera.SetFarPlane(1000.0f);

                auto transform = cameraObject.GetWorldTransform();
                // Rotate 180 degrees around Y (up), then move the camera backward a bit so the demo
                // starts with the model in view.
                transform.Rotation = Core::Math::FQuaternion::FromAxisAngle(
                    Core::Math::FVector3f(0.0f, 1.0f, 0.0f), Core::Math::kPiF);

                // Lift the camera slightly so it's not exactly at ground level.
                const Core::Math::FVector3f basePos(0.0f, 70.0f, -4.0f);
                const Core::Math::FVector3f localBackward(0.0f, 0.0f, -1.0f);
                const Core::Math::FVector3f backwardWorld =
                    transform.Rotation.RotateVector(localBackward);
                // Move the camera far back so we can see multiple CSM cascades populated.
                constexpr float kBackDistance = 250.0f;
                // NOTE: Vector supports component-wise multiply; use a splat vector for scalar
                // scaling.
                transform.Translation =
                    basePos + backwardWorld * Core::Math::FVector3f(kBackDistance);
                cameraObject.SetWorldTransform(transform);
                cameraRotationWS = transform.Rotation;
            }
            if (scriptComponent.IsValid()) {
                scriptComponent.Get().SetScriptAsset(scriptHandle);
            }

            // Skybox (asset may be missing until cooked; component still exists for demo wiring).
            {
                auto skyObject    = world->CreateGameObject(TEXT("SkyBox"));
                auto skyComponent = skyObject.AddComponent<GameScene::FSkyCubeComponent>();
                if (skyComponent.IsValid()) {
                    skyComponent.Get().SetCubeMapAsset(skyCubeHandle);
                }
            }

            // Floor (static mesh + material) at world Y=0. We author the plane in XY (z=0) so the
            // demo shader's UV mapping (Position.xy) is usable, then rotate it onto XZ.
            {
                const auto floorMeshHandle = engineLoop.GetAssetRegistry().FindByPath(
                    TEXT("demo/minimal/models/floor_plane"));
                const auto floorMaterialHandle =
                    engineLoop.GetAssetRegistry().FindByPath(TEXT("demo/minimal/materials/floor"));

                if (!floorMeshHandle.IsValid() || !floorMaterialHandle.IsValid()) {
                    LogWarning(TEXT("Floor assets missing; skipping floor (mesh or material)."));
                } else {
                    auto floorObject = world->CreateGameObject(TEXT("Floor"));

                    auto t        = floorObject.GetWorldTransform();
                    t.Translation = Core::Math::FVector3f(0.0f, 0.0f, 0.0f);
                    t.Rotation    = Core::Math::FEulerRotator(-Core::Math::kPiF * 0.5f, 0.0f, 0.0f)
                                     .ToQuaternion();
                    floorObject.SetWorldTransform(t);

                    auto meshFilter =
                        floorObject.AddComponent<GameScene::FStaticMeshFilterComponent>();
                    if (meshFilter.IsValid()) {
                        meshFilter.Get().SetStaticMeshAsset(floorMeshHandle);
                    }

                    auto materialComp =
                        floorObject.AddComponent<GameScene::FMeshMaterialComponent>();
                    if (materialComp.IsValid()) {
                        materialComp.Get().SetMaterialTemplate(0, floorMaterialHandle);
                    }
                }
            }

            // Directional light (main). Used by deferred lighting + CSM (if enabled).
            {
                // Debug: use Lambert shading in deferred lighting so the result is visible even if
                // the model doesn't have full PBR textures/parameters authored.
                Rendering::FBasicDeferredRenderer::SetDeferredLightingDebugLambert(false);

                auto lightObject = world->CreateGameObject(TEXT("DirectionalLight"));
                auto lightComponent =
                    lightObject.AddComponent<GameScene::FDirectionalLightComponent>();
                if (lightComponent.IsValid()) {
                    auto& light        = lightComponent.Get();
                    light.mColor       = Core::Math::FVector3f(1.0f, 1.0f, 1.0f);
                    light.mIntensity   = 2.0f;
                    light.mCastShadows = true;

                    // +Z is treated as light propagation direction (see component comment).
                    // Aim to the camera's right-front direction (relative to camera forward), using
                    // world +Y as up.
                    auto                        t = lightObject.GetWorldTransform();

                    const Core::Math::FVector3f forwardWS =
                        cameraRotationWS.RotateVector(Core::Math::FVector3f(0.0f, 0.0f, 1.0f));
                    const Core::Math::FVector3f rightWS =
                        cameraRotationWS.RotateVector(Core::Math::FVector3f(1.0f, 0.0f, 0.0f));

                    // Project to XZ plane so "right-front" is horizontal (world up is +Y).
                    Core::Math::FVector3f forwardXZ(forwardWS[0], 0.0f, forwardWS[2]);
                    Core::Math::FVector3f rightXZ(rightWS[0], 0.0f, rightWS[2]);

                    auto                  Normalize3 = [](Core::Math::FVector3f v) {
                        const float len2 = v[0] * v[0] + v[1] * v[1] + v[2] * v[2];
                        if (len2 <= 1e-8f) {
                            return Core::Math::FVector3f(0.0f, 0.0f, 1.0f);
                        }
                        const float invLen = 1.0f / Core::Math::Sqrt(len2);
                        return Core::Math::FVector3f(v[0] * invLen, v[1] * invLen, v[2] * invLen);
                    };

                    forwardXZ                         = Normalize3(forwardXZ);
                    rightXZ                           = Normalize3(rightXZ);
                    const Core::Math::FVector3f dirWS = Normalize3(forwardXZ + rightXZ);

                    // Our Euler yaw rotates around +Y, pitch around +X, and +Z is forward.
                    // NOTE: In our convention, +pitch rotates +Z towards -Y (i.e. "down").
                    const float                 yaw   = Core::Math::Atan2(dirWS[0], dirWS[2]);
                    const float                 pitch = Core::Math::kPiF * 0.25f; // down 45 degrees
                    t.Rotation    = Core::Math::FEulerRotator(pitch, yaw, 0.0f).ToQuaternion();
                    t.Translation = Core::Math::FVector3f(0.0f, 0.0f, 0.0f);
                    lightObject.SetWorldTransform(t);
                }
            }

            auto modelResult = Engine::GameSceneAsset::FModelAssetInstantiator::Instantiate(
                *world, assetManager, modelHandle);
            if (!modelResult.Root.IsValid()) {
                LogError(TEXT("Failed to instantiate model asset."));
                return false;
            }

            // This demo model has no normal map. Force vertex-normal shading so lighting is
            // readable without authoring a NormalTex.
            {
                const auto normalStrengthId =
                    RenderCore::HashMaterialParamName(TEXT("NormalMapStrength"));
                for (const auto compId : world->GetActiveMeshMaterialComponents()) {
                    if (!world->IsAlive(compId)) {
                        continue;
                    }
                    auto& comp = world->ResolveComponent<GameScene::FMeshMaterialComponent>(compId);
                    for (auto& slot : comp.GetMaterials()) {
                        slot.Parameters.SetScalar(normalStrengthId, 0.0f);
                    }
                }
            }

            {
                Core::Reflection::FJsonSerializer serializer;
                world->SerializeJson(serializer);
                LogInfo(TEXT("World JSON: {}"), serializer.GetString().CStr());
            }

            return true;
        }

        auto OnTick(Launch::FEngineLoop& engineLoop, float deltaSeconds) -> bool override {
            engineLoop.Tick(deltaSeconds);
            Core::Platform::Generic::PlatformSleepMilliseconds(16);
            return engineLoop.IsRunning();
        }
    };
} // namespace

int main(int argc, char** argv) {
    FStartupParameters startupParams{};
    if (argc > 1) {
        startupParams.mCommandLine = argv[1];
    }

    FMinimalGameClient client;
    return Launch::RunGameClient(client, startupParams);
}
