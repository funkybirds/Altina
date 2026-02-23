#include "Base/AltinaBase.h"
#include "Asset/AssetBinary.h"
#include "Asset/AssetManager.h"
#include "Asset/AssetRegistry.h"
#include "Engine/GameScene/CameraComponent.h"
#include "Engine/GameScene/DirectionalLightComponent.h"
#include "Engine/GameScene/ScriptComponent.h"
#include "Engine/GameScene/World.h"
#include "Engine/GameSceneAsset/ModelAssetInstantiator.h"
#include "Launch/EngineLoop.h"
#include "Launch/GameClient.h"
#include "Logging/Log.h"
#include "Material/MaterialPass.h"
#include "Container/SmartPtr.h"
#include "Math/Common.h"
#include "Math/LinAlg/SpatialTransform.h"
#include "Math/Vector.h"
#include "Platform/Generic/GenericPlatformDecl.h"
#include "Platform/PlatformFileSystem.h"
#include "Rendering/BasicDeferredRenderer.h"
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
            auto&      assetManager = engineLoop.GetAssetManager();

            const auto modelHandle = engineLoop.GetAssetRegistry().FindByPath(
                TEXT("demo/minimal/models/hoshino/hoshino_battle"));
            const auto scriptHandle =
                engineLoop.GetAssetRegistry().FindByPath(TEXT("demo/minimal/scripts/demoscript"));
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
            if (cameraComponent.IsValid()) {
                auto& camera = cameraComponent.Get();
                camera.SetNearPlane(0.1f);
                camera.SetFarPlane(1000.0f);

                auto transform = cameraObject.GetWorldTransform();
                // Rotate 180 degrees around Y (up), then move the camera backward a bit so the demo
                // starts with the model in view.
                transform.Rotation = Core::Math::FQuaternion::FromAxisAngle(
                    Core::Math::FVector3f(0.0f, 1.0f, 0.0f), Core::Math::kPiF);

                const Core::Math::FVector3f basePos(0.0f, 0.0f, -4.0f);
                const Core::Math::FVector3f localBackward(0.0f, 0.0f, -1.0f);
                const Core::Math::FVector3f backwardWorld =
                    transform.Rotation.RotateVector(localBackward);
                constexpr float kBackDistance = 8.0f;
                // NOTE: Vector supports component-wise multiply; use a splat vector for scalar
                // scaling.
                transform.Translation =
                    basePos + backwardWorld * Core::Math::FVector3f(kBackDistance);
                cameraObject.SetWorldTransform(transform);
            }
            if (scriptComponent.IsValid()) {
                scriptComponent.Get().SetScriptAsset(scriptHandle);
            }

            // Directional light (main). Used by deferred lighting + CSM (if enabled).
            {
                auto lightObject = world->CreateGameObject(TEXT("DirectionalLight"));
                auto lightComponent =
                    lightObject.AddComponent<GameScene::FDirectionalLightComponent>();
                if (lightComponent.IsValid()) {
                    auto& light        = lightComponent.Get();
                    light.mColor       = Core::Math::FVector3f(1.0f, 1.0f, 1.0f);
                    light.mIntensity   = 2.0f;
                    light.mCastShadows = true;

                    // +Z is treated as light propagation direction (see component comment).
                    // Keep identity rotation for now: propagation = (0,0,1).
                    auto t        = lightObject.GetWorldTransform();
                    t.Rotation    = Core::Math::FQuaternion::Identity();
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
