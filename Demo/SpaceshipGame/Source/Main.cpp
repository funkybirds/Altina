#include "Base/AltinaBase.h"
#include "Engine/GameScene/CameraComponent.h"
#include "Engine/GameScene/DirectionalLightComponent.h"
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
            auto&      worldManager = engineLoop.GetWorldManager();
            const auto worldHandle  = worldManager.CreateWorld();
            worldManager.SetActiveWorld(worldHandle);

            auto* world = worldManager.GetWorld(worldHandle);
            if (world == nullptr) {
                return false;
            }

            // Camera.
            {
                auto cameraObject = world->CreateGameObject(TEXT("Camera"));
                auto cameraComp   = cameraObject.AddComponent<GameScene::FCameraComponent>();
                if (cameraComp.IsValid()) {
                    cameraComp.Get().SetNearPlane(0.1f);
                    cameraComp.Get().SetFarPlane(5000.0f);
                }

                auto transform        = cameraObject.GetWorldTransform();
                transform.Translation = Core::Math::FVector3f(0.0f, 5.0f, -20.0f);
                transform.Rotation    = Core::Math::FEulerRotator(0.0f, 0.0f, 0.0f).ToQuaternion();
                cameraObject.SetWorldTransform(transform);
            }

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

    FSpaceshipGameClient client;
    return Launch::RunGameClient(client, startupParams);
}
