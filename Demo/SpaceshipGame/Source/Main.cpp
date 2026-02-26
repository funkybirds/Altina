#include "Base/AltinaBase.h"
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
#include "Math/LinAlg/SpatialTransform.h"
#include "Math/Rotation.h"
#include "Platform/Generic/GenericPlatformDecl.h"
#include "Rendering/PostProcess/PostProcessSettings.h"

using namespace AltinaEngine;

namespace {
    class FSpaceshipGameClient final : public Launch::FGameClient {
    public:
        auto OnInit(Launch::FEngineLoop& engineLoop) -> bool override {
            auto&      assetManager = engineLoop.GetAssetManager();

            const auto sphereMeshHandle =
                engineLoop.GetAssetRegistry().FindByPath(TEXT("demo/spaceshipgame/models/sphere"));

            const auto sunMaterialHandle =
                engineLoop.GetAssetRegistry().FindByPath(TEXT("demo/spaceshipgame/materials/sun"));
            const auto earthMaterialHandle = engineLoop.GetAssetRegistry().FindByPath(
                TEXT("demo/spaceshipgame/materials/earth"));
            const auto moonMaterialHandle =
                engineLoop.GetAssetRegistry().FindByPath(TEXT("demo/spaceshipgame/materials/moon"));
            const auto shipMaterialHandle =
                engineLoop.GetAssetRegistry().FindByPath(TEXT("demo/spaceshipgame/materials/ship"));

            const auto sunModelHandle =
                engineLoop.GetAssetRegistry().FindByPath(TEXT("demo/spaceshipgame/models/sun"));
            const auto earthModelHandle = engineLoop.GetAssetRegistry().FindByPath(
                TEXT("demo/spaceshipgame/models/earthsimple"));
            const auto moonModelHandle =
                engineLoop.GetAssetRegistry().FindByPath(TEXT("demo/spaceshipgame/models/moon"));
            const auto shipModelHandle =
                engineLoop.GetAssetRegistry().FindByPath(TEXT("demo/spaceshipgame/models/apollo"));

            const auto skyCubeHandle = engineLoop.GetAssetRegistry().FindByPath(
                TEXT("demo/spaceshipgame/skyboxes/nebulae"));

            const auto shipScriptHandle = engineLoop.GetAssetRegistry().FindByPath(
                TEXT("demo/spaceshipgame/scripts/shiporbit"));
            const auto cameraScriptHandle = engineLoop.GetAssetRegistry().FindByPath(
                TEXT("demo/spaceshipgame/scripts/shipcameramodes"));
            const auto earthScriptHandle = engineLoop.GetAssetRegistry().FindByPath(
                TEXT("demo/spaceshipgame/scripts/earthrevolve"));
            const auto moonScriptHandle = engineLoop.GetAssetRegistry().FindByPath(
                TEXT("demo/spaceshipgame/scripts/moonrevolve"));

            if (!sphereMeshHandle.IsValid() || !sunMaterialHandle.IsValid()
                || !earthMaterialHandle.IsValid() || !moonMaterialHandle.IsValid()
                || !shipMaterialHandle.IsValid() || !skyCubeHandle.IsValid()
                || !sunModelHandle.IsValid() || !earthModelHandle.IsValid()
                || !moonModelHandle.IsValid() || !shipModelHandle.IsValid()
                || !shipScriptHandle.IsValid() || !cameraScriptHandle.IsValid()
                || !earthScriptHandle.IsValid() || !moonScriptHandle.IsValid()) {
                return false;
            }

            mMouseLocked = true;

            // Enable bloom post-process for the demo (defaults are defined in
            // PostProcessSettings.cpp).
            Rendering::rPostProcessBloom.Set(1);
            Rendering::rPostProcessBloomThreshold.Set(1.0f);
            Rendering::rPostProcessBloomKnee.Set(0.5f);
            Rendering::rPostProcessBloomIntensity.Set(0.20f);
            Rendering::rPostProcessBloomIterations.Set(5);
            Rendering::rPostProcessBloomKawaseOffset.Set(1.0f);
            LogInfo(TEXT("[SpaceshipGame] PostProcess: Bloom=ON (press B to toggle)."));

            auto&      worldManager = engineLoop.GetWorldManager();
            const auto worldHandle  = worldManager.CreateWorld();
            worldManager.SetActiveWorld(worldHandle);

            auto* world = worldManager.GetWorld(worldHandle);
            if (world == nullptr) {
                return false;
            }

            // Skybox.
            {
                auto skyObject    = world->CreateGameObject(TEXT("SkyBox"));
                auto skyComponent = skyObject.AddComponent<GameScene::FSkyCubeComponent>();
                if (skyComponent.IsValid()) {
                    skyComponent.Get().SetCubeMapAsset(skyCubeHandle);
                }
            }

            // Ship root (logic + transform). Keep renderable mesh on a child so hiding the ship
            // does not accidentally break camera transforms by setting parent scale to zero.
            GameScene::FGameObjectView shipRootObject;
            {
                shipRootObject = world->CreateGameObject(TEXT("Ship"));
                auto t         = shipRootObject.GetWorldTransform();
                t.Translation  = Core::Math::FVector3f(401.2f, 0.0f, 0.0f);
                t.Scale        = Core::Math::FVector3f(1.0f);
                shipRootObject.SetWorldTransform(t);

                auto scriptComp = shipRootObject.AddComponent<GameScene::FScriptComponent>();
                if (scriptComp.IsValid()) {
                    scriptComp.Get().SetScriptAsset(shipScriptHandle);
                }
            }

            // Ship visual (model).
            {
                auto shipVisualObject = world->CreateGameObject(TEXT("ShipVisual"));
                shipVisualObject.SetParent(shipRootObject.GetId());

                auto t = shipVisualObject.GetLocalTransform();
                // Computed by AssetTool modelinfo so Apollo.usdz extent radius matches ShipRadius.
                t.Scale = Core::Math::FVector3f(0.000255973f);
                shipVisualObject.SetLocalTransform(t);

                auto shipModelResult = Engine::GameSceneAsset::FModelAssetInstantiator::Instantiate(
                    *world, assetManager, shipModelHandle);
                if (shipModelResult.Root.IsValid()) {
                    world->Object(shipModelResult.Root).SetParent(shipVisualObject.GetId());

                    for (const auto nodeId : shipModelResult.Nodes) {
                        if (!nodeId.IsValid()) {
                            continue;
                        }
                        auto nodeView = world->Object(nodeId);
                        if (!nodeView.HasComponent<GameScene::FStaticMeshFilterComponent>()) {
                            continue;
                        }

                        auto matComp = nodeView.GetComponent<GameScene::FMeshMaterialComponent>();
                        if (!matComp.IsValid()) {
                            continue;
                        }
                        const u32 slotCount = matComp.Get().GetMaterialCount();
                        const u32 setCount  = (slotCount > 0U) ? slotCount : 1U;
                        for (u32 slot = 0; slot < setCount; ++slot) {
                            matComp.Get().SetMaterialTemplate(slot, shipMaterialHandle);
                        }
                    }
                } else {
                    LogWarning(TEXT("[SpaceshipGame] Failed to instantiate ship model."));
                }
            }

            // Camera (first-person + third-person, toggled in script).
            {
                auto cameraObject = world->CreateGameObject(TEXT("Camera"));
                cameraObject.SetParent(shipRootObject.GetId());

                auto cameraComp = cameraObject.AddComponent<GameScene::FCameraComponent>();
                if (cameraComp.IsValid()) {
                    cameraComp.Get().SetNearPlane(0.1f);
                    cameraComp.Get().SetFarPlane(5000.0f);
                }

                auto scriptComp = cameraObject.AddComponent<GameScene::FScriptComponent>();
                if (scriptComp.IsValid()) {
                    scriptComp.Get().SetScriptAsset(cameraScriptHandle);
                }
            }

            auto CreateBodyFromModel =
                [&](Core::Container::FStringView name, const Asset::FAssetHandle model,
                    const Asset::FAssetHandle overrideMaterial, const Core::Math::FVector3f& pos,
                    float uniformScale) -> GameScene::FGameObjectView {
                auto obj      = world->CreateGameObject(name);
                auto t        = obj.GetWorldTransform();
                t.Translation = pos;
                t.Scale       = Core::Math::FVector3f(uniformScale);
                obj.SetWorldTransform(t);

                auto modelResult = Engine::GameSceneAsset::FModelAssetInstantiator::Instantiate(
                    *world, assetManager, model);
                if (!modelResult.Root.IsValid()) {
                    LogWarning(TEXT("[SpaceshipGame] Failed to instantiate model for '{}'."), name);
                    return obj;
                }

                world->Object(modelResult.Root).SetParent(obj.GetId());

                // Force a single demo material so we can reuse the extracted textures/PBR preset
                // regardless of the model's embedded material slots.
                u32 meshNodeCount = 0;
                for (const auto nodeId : modelResult.Nodes) {
                    if (!nodeId.IsValid()) {
                        continue;
                    }
                    auto nodeView = world->Object(nodeId);
                    if (!nodeView.HasComponent<GameScene::FStaticMeshFilterComponent>()) {
                        continue;
                    }
                    ++meshNodeCount;

                    auto matComp = nodeView.GetComponent<GameScene::FMeshMaterialComponent>();
                    if (!matComp.IsValid()) {
                        continue;
                    }
                    const u32 slotCount = matComp.Get().GetMaterialCount();
                    const u32 setCount  = (slotCount > 0U) ? slotCount : 1U;
                    for (u32 slot = 0; slot < setCount; ++slot) {
                        matComp.Get().SetMaterialTemplate(slot, overrideMaterial);
                    }
                }

                LogInfo(TEXT("[SpaceshipGame] Instantiated '{}' model: nodes={}, meshNodes={}."),
                    name, static_cast<u32>(modelResult.Nodes.Size()), meshNodeCount);
                return obj;
            };

            // Suggested scales are computed by AssetTool modelinfo so imported USDZ extents match
            // Milestone 3 radii (world unit: 1 = 10,000 km).
            auto sunObject   = CreateBodyFromModel(TEXT("Sun"), sunModelHandle, sunMaterialHandle,
                  Core::Math::FVector3f(0.0f, 0.0f, 0.0f), 0.0464227f * 1.0f);
            auto earthObject = CreateBodyFromModel(TEXT("Earth"), earthModelHandle,
                earthMaterialHandle, Core::Math::FVector3f(400.0f, 0.0f, 0.0f), 0.00515107f * 1.0f);
            auto moonObject = CreateBodyFromModel(TEXT("Moon"), moonModelHandle, moonMaterialHandle,
                Core::Math::FVector3f(438.44f, 0.0f, 0.0f), 0.001158f * 1.0f);

            if (sunObject.IsValid()) {
                mSunObjectId = sunObject.GetId();
            }
            if (earthObject.IsValid()) {
                mEarthObjectId = earthObject.GetId();
            }

            // Celestial revolution scripts (Milestone 3).
            {
                if (earthObject.IsValid()) {
                    auto earthScript = earthObject.AddComponent<GameScene::FScriptComponent>();
                    if (earthScript.IsValid()) {
                        earthScript.Get().SetScriptAsset(earthScriptHandle);
                    }
                }

                if (moonObject.IsValid()) {
                    auto moonScript = moonObject.AddComponent<GameScene::FScriptComponent>();
                    if (moonScript.IsValid()) {
                        moonScript.Get().SetScriptAsset(moonScriptHandle);
                    }
                }
            }

            // Directional light.
            {
                auto lightObject          = world->CreateGameObject(TEXT("DirectionalLight"));
                mDirectionalLightObjectId = lightObject.GetId();
                auto lightComp = lightObject.AddComponent<GameScene::FDirectionalLightComponent>();
                if (lightComp.IsValid()) {
                    auto& light        = lightComp.Get();
                    light.mColor       = Core::Math::FVector3f(1.0f, 1.0f, 1.0f);
                    light.mIntensity   = 2.0f;
                    light.mCastShadows = true;
                }

                // Initialize to "Sun -> Earth" direction.
                UpdateSunLightDirection(engineLoop);
            }

            return true;
        }

        auto OnTick(Launch::FEngineLoop& engineLoop, float deltaSeconds) -> bool override {
            engineLoop.Tick(deltaSeconds);

            UpdateSunLightDirection(engineLoop);

            auto* input  = engineLoop.GetInputSystem();
            auto* window = engineLoop.GetMainWindow();
            if (input != nullptr && window != nullptr) {
                // Demo-only FPS mouse lock toggle. Escape releases / re-captures the mouse.
                if (input->WasKeyPressed(Input::EKey::Escape)) {
                    mMouseLocked = !mMouseLocked;
                }

                if (input->WasKeyPressed(Input::EKey::B)) {
                    const i32 enabled = (Rendering::rPostProcessBloom.Get() != 0) ? 0 : 1;
                    Rendering::rPostProcessBloom.Set(enabled);
                    LogInfo(TEXT("[SpaceshipGame] PostProcess: Bloom={}."),
                        enabled ? TEXT("ON") : TEXT("OFF"));
                }

                if (mMouseLocked && input->HasFocus()) {
                    window->SetCursorVisible(false);
                    window->SetCursorClippedToClient(true);

                    const u32 width  = input->GetWindowWidth();
                    const u32 height = input->GetWindowHeight();
                    if (width > 0U && height > 0U) {
                        const i32 cx = static_cast<i32>(width / 2U);
                        const i32 cy = static_cast<i32>(height / 2U);

                        // Prevent warp-generated mouse move events from producing artificial
                        // deltas.
                        input->SetMousePositionNoDelta(cx, cy);
                        window->SetCursorPositionClient(cx, cy);
                    }
                } else {
                    window->SetCursorClippedToClient(false);
                    window->SetCursorVisible(true);
                }
            }

            Core::Platform::Generic::PlatformSleepMilliseconds(16);
            return engineLoop.IsRunning();
        }

        void OnShutdown(Launch::FEngineLoop& engineLoop) override {
            if (auto* window = engineLoop.GetMainWindow()) {
                window->SetCursorClippedToClient(false);
                window->SetCursorVisible(true);
            }
        }

    private:
        void UpdateSunLightDirection(Launch::FEngineLoop& engineLoop) {
            auto* world = engineLoop.GetWorldManager().GetActiveWorld();
            if (world == nullptr) {
                return;
            }

            if (!mDirectionalLightObjectId.IsValid() || !mSunObjectId.IsValid()
                || !mEarthObjectId.IsValid()) {
                return;
            }

            auto       lightObj = world->Object(mDirectionalLightObjectId);
            const auto sunObj   = world->Object(mSunObjectId);
            const auto earthObj = world->Object(mEarthObjectId);
            if (!lightObj.IsValid() || !sunObj.IsValid() || !earthObj.IsValid()) {
                return;
            }

            const auto sunPos   = sunObj.GetWorldTransform().Translation;
            const auto earthPos = earthObj.GetWorldTransform().Translation;

            auto       dir = earthPos - sunPos;
            dir.Y()        = 0.0f;

            const f32 len2 = dir.X() * dir.X() + dir.Z() * dir.Z();
            if (len2 <= 1e-6f) {
                return;
            }

            const f32 invLen = 1.0f / Core::Math::Sqrt(len2);
            dir              = dir * Core::Math::FVector3f(invLen, invLen, invLen);

            // Light direction is derived from the owner's +Z axis.
            const f32 yaw = Core::Math::Atan2(dir.X(), dir.Z());

            auto      t = lightObj.GetWorldTransform();
            t.Rotation  = Core::Math::FEulerRotator(0.0f, yaw, 0.0f).ToQuaternion();
            lightObj.SetWorldTransform(t);
        }

        bool                     mMouseLocked = false;
        GameScene::FGameObjectId mSunObjectId{};
        GameScene::FGameObjectId mEarthObjectId{};
        GameScene::FGameObjectId mDirectionalLightObjectId{};
    };
} // namespace

int main(int argc, char** argv) {
    FStartupParameters startupParams{};
    if (argc > 1) {
        startupParams.mCommandLine = argv[1];
    }

    FSpaceshipGameClient client;
    return Launch::RunGameClient(client, startupParams);
}
