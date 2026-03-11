#include "Base/AltinaBase.h"
#include "Asset/AssetManager.h"
#include "Asset/AssetRegistry.h"
#include "Engine/GameScene/CameraComponent.h"
#include "Engine/GameScene/MeshMaterialComponent.h"
#include "Engine/GameScene/World.h"
#include "Engine/GameSceneAsset/LevelAssetIO.h"
#include "Launch/EngineLoop.h"
#include "Launch/DemoRuntime.h"
#include "Launch/GameClient.h"
#include "Logging/Log.h"
#include "Input/InputSystem.h"
#include "Math/Common.h"
#include "Math/LinAlg/SpatialTransform.h"
#include "Math/Rotation.h"
#include "Math/Vector.h"
#include "Rendering/PostProcess/PostProcessSettings.h"
#include "Types/Aliases.h"

using namespace AltinaEngine;

namespace {
    class FMinimalGameClient final : public Launch::FGameClient {
    public:
        auto OnInit(Launch::FEngineLoop& engineLoop) -> bool override {
            auto& assetManager = engineLoop.GetAssetManager();

            // Enable FXAA in the Minimal demo so RenderDoc captures show the pass by default.
            Rendering::rPostProcessFxaa.Set(1);

            const auto levelHandle =
                engineLoop.GetAssetRegistry().FindByPath(TEXT("demo/minimal/levels/default"));
            if (!levelHandle.IsValid()) {
                LogError(TEXT("Demo level asset missing: demo/minimal/levels/default."));
                return false;
            }

            auto& worldManager = engineLoop.GetWorldManager();
            auto  loadedWorld =
                Engine::GameSceneAsset::LoadWorldFromLevelAsset(levelHandle, assetManager);
            if (!loadedWorld) {
                LogError(TEXT("Failed to deserialize level asset as world."));
                return false;
            }

            const auto worldHandle = worldManager.AddWorld(Move(loadedWorld));
            if (!worldHandle.IsValid()) {
                LogError(TEXT("Failed to add level world into world manager."));
                return false;
            }

            worldManager.SetActiveWorld(worldHandle);
            auto* world = worldManager.GetWorld(worldHandle);
            if (world == nullptr) {
                LogError(TEXT("Failed to acquire world from world manager."));
                return false;
            }

            mWorldHandle        = worldHandle;
            bool resolvedCamera = false;
            for (const auto cameraCompId : world->GetActiveCameraComponents()) {
                if (!world->IsAlive(cameraCompId)) {
                    continue;
                }

                const auto& cameraComp =
                    world->ResolveComponent<GameScene::FCameraComponent>(cameraCompId);
                const auto cameraObjectId = cameraComp.GetOwner();
                if (!world->IsAlive(cameraObjectId)) {
                    continue;
                }

                mCameraObjectId      = cameraObjectId;
                const auto transform = world->Object(cameraObjectId).GetWorldTransform();
                const Core::Math::FEulerRotator euler(transform.Rotation);
                mCameraPitchRadians = euler.pitch;
                mCameraYawRadians   = euler.yaw;
                resolvedCamera      = true;
                break;
            }
            if (!resolvedCamera) {
                LogError(TEXT("No active camera found in level world."));
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

            return true;
        }

        auto OnTick(Launch::FEngineLoop& engineLoop, float deltaSeconds) -> bool override {
            // Camera controls:
            // - RMB + mouse delta: look around.
            // - WASD + QE: translate camera in local space (+ world up/down for QE).
            // NOTE: We apply input before EngineLoop::Tick clears platform frame state.
            if (const auto* input = engineLoop.GetInputSystem();
                input != nullptr && input->HasFocus()) {
                UpdateCameraKeyboardMove(engineLoop, *input, deltaSeconds);
                constexpr u32 kRmb = 1U;
                if (input->IsMouseButtonDown(kRmb)) {
                    UpdateCameraMouseLook(engineLoop, *input);
                }
            }

            (void)deltaSeconds;
            return engineLoop.IsRunning();
        }

    private:
        void UpdateCameraMouseLook(
            Launch::FEngineLoop& engineLoop, const Input::FInputSystem& input) noexcept {
            auto& worldManager = engineLoop.GetWorldManager();
            auto* world        = worldManager.GetWorld(mWorldHandle);
            if (world == nullptr || !world->IsAlive(mCameraObjectId)) {
                return;
            }

            const i32 dx = input.GetMouseDeltaX();
            const i32 dy = input.GetMouseDeltaY();
            if (dx == 0 && dy == 0) {
                return;
            }

            // Sensitivity is radians per pixel.
            constexpr f32 kSensitivity = 0.0025f;
            mCameraYawRadians += static_cast<f32>(dx) * kSensitivity;
            mCameraPitchRadians -= static_cast<f32>(dy) * kSensitivity;

            // Clamp pitch to avoid flipping.
            constexpr f32 kPitchLimit = Core::Math::kHalfPiF - 0.01f;
            if (mCameraPitchRadians > kPitchLimit) {
                mCameraPitchRadians = kPitchLimit;
            } else if (mCameraPitchRadians < -kPitchLimit) {
                mCameraPitchRadians = -kPitchLimit;
            }

            auto obj       = world->Object(mCameraObjectId);
            auto transform = obj.GetWorldTransform();
            transform.Rotation =
                Core::Math::FEulerRotator(mCameraPitchRadians, mCameraYawRadians, 0.0f)
                    .ToQuaternion();
            obj.SetWorldTransform(transform);
        }

        void UpdateCameraKeyboardMove(Launch::FEngineLoop& engineLoop,
            const Input::FInputSystem& input, f32 deltaSeconds) noexcept {
            auto& worldManager = engineLoop.GetWorldManager();
            auto* world        = worldManager.GetWorld(mWorldHandle);
            if (world == nullptr || !world->IsAlive(mCameraObjectId)) {
                return;
            }

            f32 moveForward = 0.0f;
            f32 moveRight   = 0.0f;
            f32 moveUp      = 0.0f;
            if (input.IsKeyDown(Input::EKey::W)) {
                moveForward += 1.0f;
            }
            if (input.IsKeyDown(Input::EKey::S)) {
                moveForward -= 1.0f;
            }
            if (input.IsKeyDown(Input::EKey::D)) {
                moveRight += 1.0f;
            }
            if (input.IsKeyDown(Input::EKey::A)) {
                moveRight -= 1.0f;
            }
            if (input.IsKeyDown(Input::EKey::E)) {
                moveUp += 1.0f;
            }
            if (input.IsKeyDown(Input::EKey::Q)) {
                moveUp -= 1.0f;
            }

            if (moveForward == 0.0f && moveRight == 0.0f && moveUp == 0.0f) {
                return;
            }

            auto       obj       = world->Object(mCameraObjectId);
            auto       transform = obj.GetWorldTransform();

            const auto forward =
                transform.Rotation.RotateVector(Core::Math::FVector3f(0.0f, 0.0f, 1.0f));
            const auto right =
                transform.Rotation.RotateVector(Core::Math::FVector3f(1.0f, 0.0f, 0.0f));
            const auto up = Core::Math::FVector3f(0.0f, 1.0f, 0.0f);

            auto       moveDir = forward * Core::Math::FVector3f(moveForward)
                + right * Core::Math::FVector3f(moveRight) + up * Core::Math::FVector3f(moveUp);
            const f32 len2 =
                moveDir[0] * moveDir[0] + moveDir[1] * moveDir[1] + moveDir[2] * moveDir[2];
            if (len2 > 1e-8f) {
                const f32 invLen = 1.0f / Core::Math::Sqrt(len2);
                moveDir          = Core::Math::FVector3f(
                    moveDir[0] * invLen, moveDir[1] * invLen, moveDir[2] * invLen);
            }

            constexpr f32 kMoveSpeed = 140.0f;
            transform.Translation += moveDir * Core::Math::FVector3f(kMoveSpeed * deltaSeconds);
            obj.SetWorldTransform(transform);
        }

        GameScene::FWorldHandle  mWorldHandle{};
        GameScene::FGameObjectId mCameraObjectId{};
        f32                      mCameraPitchRadians = 0.0f;
        f32                      mCameraYawRadians   = 0.0f;
    };
} // namespace

namespace {
    class FMinimalDemoClientAdapter final : public Launch::IDemoClient {
    public:
        auto OnPreInit(Launch::FDemoRuntimeContext& context) -> bool override {
            auto* engineLoop = dynamic_cast<Launch::FEngineLoop*>(&context.Session);
            if (engineLoop == nullptr) {
                return false;
            }
            return mClient.OnPreInit(*engineLoop);
        }

        auto OnInit(Launch::FDemoRuntimeContext& context) -> bool override {
            auto* engineLoop = dynamic_cast<Launch::FEngineLoop*>(&context.Session);
            if (engineLoop == nullptr) {
                return false;
            }
            return mClient.OnInit(*engineLoop);
        }

        auto OnTick(Launch::FDemoRuntimeContext& context, f32 deltaSeconds) -> bool override {
            auto* engineLoop = dynamic_cast<Launch::FEngineLoop*>(&context.Session);
            if (engineLoop == nullptr) {
                return false;
            }
            return mClient.OnTick(*engineLoop, deltaSeconds);
        }

        void OnShutdown(Launch::FDemoRuntimeContext& context) override {
            auto* engineLoop = dynamic_cast<Launch::FEngineLoop*>(&context.Session);
            if (engineLoop == nullptr) {
                return;
            }
            mClient.OnShutdown(*engineLoop);
        }

    private:
        FMinimalGameClient mClient{};
    };
} // namespace

extern "C" AE_DLLEXPORT auto AE_CreateDemoClient() -> Launch::IDemoClient* {
    return new FMinimalDemoClientAdapter();
}

extern "C" AE_DLLEXPORT void AE_DestroyDemoClient(Launch::IDemoClient* client) { delete client; }
