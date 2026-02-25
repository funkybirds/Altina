#include "Base/AltinaBase.h"
#include "Asset/AssetRegistry.h"
#include "Engine/GameScene/CameraComponent.h"
#include "Engine/GameScene/DirectionalLightComponent.h"
#include "Engine/GameScene/MeshMaterialComponent.h"
#include "Engine/GameScene/ScriptComponent.h"
#include "Engine/GameScene/StaticMeshFilterComponent.h"
#include "Engine/GameScene/World.h"
#include "Launch/EngineLoop.h"
#include "Launch/GameClient.h"
#include "Math/LinAlg/SpatialTransform.h"
#include "Math/Rotation.h"
#include "Platform/Generic/GenericPlatformDecl.h"

using namespace AltinaEngine;

namespace {
    class FSpaceshipGameClient final : public Launch::FGameClient {
    public:
        auto OnInit(Launch::FEngineLoop& engineLoop) -> bool override {
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

            const auto shipScriptHandle = engineLoop.GetAssetRegistry().FindByPath(
                TEXT("demo/spaceshipgame/scripts/shipfreemove"));
            const auto cameraScriptHandle = engineLoop.GetAssetRegistry().FindByPath(
                TEXT("demo/spaceshipgame/scripts/fpscamera"));

            if (!sphereMeshHandle.IsValid() || !sunMaterialHandle.IsValid()
                || !earthMaterialHandle.IsValid() || !moonMaterialHandle.IsValid()
                || !shipMaterialHandle.IsValid() || !shipScriptHandle.IsValid()
                || !cameraScriptHandle.IsValid()) {
                return false;
            }

            mMouseLocked = true;

            auto&      worldManager = engineLoop.GetWorldManager();
            const auto worldHandle  = worldManager.CreateWorld();
            worldManager.SetActiveWorld(worldHandle);

            auto* world = worldManager.GetWorld(worldHandle);
            if (world == nullptr) {
                return false;
            }

            // Ship.
            GameScene::FGameObjectView shipObject;
            {
                shipObject    = world->CreateGameObject(TEXT("Ship"));
                auto t        = shipObject.GetWorldTransform();
                t.Translation = Core::Math::FVector3f(20.0f, 0.0f, 6.0f);
                t.Scale       = Core::Math::FVector3f(0.6f);
                shipObject.SetWorldTransform(t);

                auto meshFilter = shipObject.AddComponent<GameScene::FStaticMeshFilterComponent>();
                if (meshFilter.IsValid()) {
                    meshFilter.Get().SetStaticMeshAsset(sphereMeshHandle);
                }

                auto materialComp = shipObject.AddComponent<GameScene::FMeshMaterialComponent>();
                if (materialComp.IsValid()) {
                    materialComp.Get().SetMaterialTemplate(0, shipMaterialHandle);
                }

                auto scriptComp = shipObject.AddComponent<GameScene::FScriptComponent>();
                if (scriptComp.IsValid()) {
                    scriptComp.Get().SetScriptAsset(shipScriptHandle);
                }
            }

            // Camera (first-person).
            {
                auto cameraObject = world->CreateGameObject(TEXT("Camera"));
                cameraObject.SetParent(shipObject.GetId());

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

            auto CreateBody = [&](Core::Container::FStringView name, const Asset::FAssetHandle mesh,
                                  const Asset::FAssetHandle    material,
                                  const Core::Math::FVector3f& pos, float scale) {
                auto obj      = world->CreateGameObject(name);
                auto t        = obj.GetWorldTransform();
                t.Translation = pos;
                t.Scale       = Core::Math::FVector3f(scale);
                obj.SetWorldTransform(t);

                auto meshFilter = obj.AddComponent<GameScene::FStaticMeshFilterComponent>();
                if (meshFilter.IsValid()) {
                    meshFilter.Get().SetStaticMeshAsset(mesh);
                }

                auto materialComp = obj.AddComponent<GameScene::FMeshMaterialComponent>();
                if (materialComp.IsValid()) {
                    materialComp.Get().SetMaterialTemplate(0, material);
                }
            };

            CreateBody(TEXT("Sun"), sphereMeshHandle, sunMaterialHandle,
                Core::Math::FVector3f(0.0f, 0.0f, 0.0f), 6.0f);
            CreateBody(TEXT("Earth"), sphereMeshHandle, earthMaterialHandle,
                Core::Math::FVector3f(20.0f, 0.0f, 0.0f), 2.0f);
            CreateBody(TEXT("Moon"), sphereMeshHandle, moonMaterialHandle,
                Core::Math::FVector3f(26.0f, 0.0f, 0.0f), 0.8f);

            // Directional light.
            {
                auto lightObject = world->CreateGameObject(TEXT("DirectionalLight"));
                auto lightComp = lightObject.AddComponent<GameScene::FDirectionalLightComponent>();
                if (lightComp.IsValid()) {
                    auto& light        = lightComp.Get();
                    light.mColor       = Core::Math::FVector3f(1.0f, 1.0f, 1.0f);
                    light.mIntensity   = 2.0f;
                    light.mCastShadows = false;
                }

                // +Z is treated as light propagation direction; tilt downward.
                auto        transform = lightObject.GetWorldTransform();
                const float pitch     = Core::Math::kPiF * 0.35f;
                const float yaw       = Core::Math::kPiF * 0.25f;
                transform.Rotation    = Core::Math::FEulerRotator(pitch, yaw, 0.0f).ToQuaternion();
                lightObject.SetWorldTransform(transform);
            }

            return true;
        }

        auto OnTick(Launch::FEngineLoop& engineLoop, float deltaSeconds) -> bool override {
            engineLoop.Tick(deltaSeconds);

            auto* input  = engineLoop.GetInputSystem();
            auto* window = engineLoop.GetMainWindow();
            if (input != nullptr && window != nullptr) {
                // Demo-only FPS mouse lock toggle. Escape releases / re-captures the mouse.
                if (input->WasKeyPressed(Input::EKey::Escape)) {
                    mMouseLocked = !mMouseLocked;
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
        bool mMouseLocked = false;
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
