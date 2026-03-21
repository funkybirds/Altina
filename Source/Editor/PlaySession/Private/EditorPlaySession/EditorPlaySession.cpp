#include "EditorPlaySession/EditorPlaySession.h"

#include "Application/PlatformWindow.h"
#include "Engine/GameScene/CameraComponent.h"
#include "Engine/GameScene/GameObjectView.h"
#include "Engine/GameScene/World.h"
#include "Engine/GameSceneAsset/LevelAssetIO.h"
#include "Input/InputSystem.h"
#include "Input/Keys.h"
#include "Launch/RuntimeSession.h"
#include "Logging/Log.h"
#include "Math/Common.h"
#include "Math/LinAlg/LookAt.h"

namespace AltinaEngine::Editor::PlaySession {
    namespace {
        using Core::Logging::LogInfoCat;
        using Core::Math::Clamp;
        using Core::Math::FEulerRotator;
        using Core::Math::FVector3f;
        using GameScene::EWorldDeserializeMode;
        using RenderCore::View::ECameraProjectionType;

        constexpr auto     kEditorInputTraceCategory = TEXT("Editor.PlaySession.Input");
        constexpr u32      kMouseButtonRight         = 1U;
        constexpr f32      kEditorCameraMoveSpeed    = 120.0f;
        constexpr f32      kEditorCameraBoostScale   = 4.0f;
        constexpr f32      kEditorCameraLookSpeed    = 0.0025f;
        constexpr f32      kEditorCameraPitchLimit   = Core::Math::kHalfPiF - 0.01f;

        [[nodiscard]] auto IsViewportInteractive(const FEditorViewportInteraction& viewport)
            -> bool {
            return viewport.bHasContent && !viewport.bUiBlockingInput
                && (viewport.bHovered || viewport.bFocused);
        }
    } // namespace

    void FEditorPlaySession::HandleFrameInput(
        const Input::FInputSystem* inputSystem, bool allowHotkeys) {
        if (inputSystem == nullptr || !allowHotkeys) {
            return;
        }

        if (inputSystem->WasKeyPressed(Input::EKey::F6)) {
            RequestPause();
        }
        if (inputSystem->WasKeyPressed(Input::EKey::F10)) {
            RequestStep();
        }
    }

    auto FEditorPlaySession::RequestPlay(Launch::IRuntimeSession& session) -> bool {
        if (GetState() == Launch::EEditorRuntimeState::Running) {
            return true;
        }

        auto services = session.GetServices();
        if (services.WorldManager == nullptr || services.AssetManager == nullptr) {
            return false;
        }

        auto  editorHandle = services.WorldManager->GetActiveWorldHandle();
        auto* editorWorld  = services.WorldManager->GetActiveWorld();
        if (!editorHandle.IsValid() || editorWorld == nullptr) {
            return false;
        }

        Core::Container::FNativeString worldSnapshot{};
        if (!Engine::GameSceneAsset::SaveWorldAsLevelJson(*editorWorld, worldSnapshot)) {
            return false;
        }

        auto runtimeWorld = GameScene::FWorld::DeserializeJson(
            worldSnapshot.ToView(), *services.AssetManager, EWorldDeserializeMode::NormalRuntime);
        if (!runtimeWorld) {
            return false;
        }

        CaptureEditorCameraForPlay();

        auto runtimeHandle =
            services.WorldManager->ReplaceWorld(editorHandle, AltinaEngine::Move(runtimeWorld));
        if (!runtimeHandle.IsValid()) {
            return false;
        }

        services.WorldManager->SetActiveWorld(runtimeHandle);
        mPlayWorldState.mWorldSnapshot      = AltinaEngine::Move(worldSnapshot);
        mPlayWorldState.mEditorWorldHandle  = editorHandle;
        mPlayWorldState.mRuntimeWorldHandle = runtimeHandle;
        mPlayWorldState.bHasSnapshot        = true;
        mController.RequestPlay();
        return true;
    }

    void FEditorPlaySession::RequestPause() {}

    void FEditorPlaySession::RequestStep() {}

    auto FEditorPlaySession::RequestStop(Launch::IRuntimeSession& session) -> bool {
        if (!mPlayWorldState.bHasSnapshot) {
            return false;
        }

        auto services = session.GetServices();
        if (services.WorldManager == nullptr || services.AssetManager == nullptr) {
            return false;
        }

        auto restoredWorld =
            GameScene::FWorld::DeserializeJson(mPlayWorldState.mWorldSnapshot.ToView(),
                *services.AssetManager, EWorldDeserializeMode::EditorRestore);
        if (!restoredWorld) {
            return false;
        }

        const auto restoredHandle = services.WorldManager->ReplaceWorld(
            mPlayWorldState.mEditorWorldHandle, AltinaEngine::Move(restoredWorld));
        if (!restoredHandle.IsValid()) {
            return false;
        }

        services.WorldManager->SetActiveWorld(restoredHandle);
        mEditorCamera   = mPlayWorldState.mSavedEditorCamera;
        mPlayWorldState = {};
        mController.OnSessionShutdown();
        return true;
    }

    auto FEditorPlaySession::ShouldTickSimulation() const -> bool {
        return mController.ShouldTickSimulation();
    }

    auto FEditorPlaySession::BuildSimulationTick(const Launch::FFrameContext& frameContext) const
        -> Launch::FSimulationTick {
        return mController.BuildSimulationTick(frameContext.DeltaSeconds);
    }

    auto FEditorPlaySession::BuildRenderTick(const FEditorViewportInteraction& viewport) const
        -> Launch::FRenderTick {
        Launch::FRenderTick tick{};
        if (viewport.bHasContent && viewport.mWidth > 0U && viewport.mHeight > 0U) {
            tick.RenderWidth  = viewport.mWidth;
            tick.RenderHeight = viewport.mHeight;
        }
        if (GetState() != Launch::EEditorRuntimeState::Running && mEditorCamera.bInitialized) {
            tick.bUseExternalPrimaryCamera = true;
            tick.ExternalPrimaryCamera     = mEditorCamera.mCamera;
        }
        return tick;
    }

    void FEditorPlaySession::UpdateEditorCamera(const Input::FInputSystem* platformInput,
        Application::FPlatformWindow* mainWindow, const FEditorViewportInteraction& viewport,
        const Launch::FFrameContext& frameContext) {
        if (GetState() == Launch::EEditorRuntimeState::Running) {
            RestoreCursorCapture(mainWindow);
            mEditorMoveInput = {};
            return;
        }

        if (platformInput == nullptr || mainWindow == nullptr || !mEditorCamera.bInitialized) {
            return;
        }

        if (platformInput->WasMouseButtonPressed(kMouseButtonRight)) {
            mEditorMoveInput.bRightMouseHeld = true;
            LogInfoCat(kEditorInputTraceCategory, TEXT("Stopped camera input: RMB pressed"));
        }
        if (platformInput->WasMouseButtonReleased(kMouseButtonRight)) {
            mEditorMoveInput.bRightMouseHeld = false;
            LogInfoCat(kEditorInputTraceCategory, TEXT("Stopped camera input: RMB released"));
        }

        if (platformInput->WasKeyPressed(Input::EKey::W)) {
            LogInfoCat(kEditorInputTraceCategory, TEXT("Stopped camera input: W pressed"));
            mEditorMoveInput.bMoveForward = true;
            LogInfoCat(kEditorInputTraceCategory,
                TEXT("Stopped camera state after W press: W={} A={} S={} D={} Boost={}"),
                mEditorMoveInput.bMoveForward ? 1U : 0U, mEditorMoveInput.bMoveLeft ? 1U : 0U,
                mEditorMoveInput.bMoveBackward ? 1U : 0U, mEditorMoveInput.bMoveRight ? 1U : 0U,
                mEditorMoveInput.bSpeedBoost ? 1U : 0U);
        }
        if (platformInput->WasKeyReleased(Input::EKey::W)) {
            LogInfoCat(kEditorInputTraceCategory, TEXT("Stopped camera input: W released"));
            mEditorMoveInput.bMoveForward = false;
            LogInfoCat(kEditorInputTraceCategory,
                TEXT("Stopped camera state after W release: W={} A={} S={} D={} Boost={}"),
                mEditorMoveInput.bMoveForward ? 1U : 0U, mEditorMoveInput.bMoveLeft ? 1U : 0U,
                mEditorMoveInput.bMoveBackward ? 1U : 0U, mEditorMoveInput.bMoveRight ? 1U : 0U,
                mEditorMoveInput.bSpeedBoost ? 1U : 0U);
        }
        if (platformInput->WasKeyPressed(Input::EKey::A)) {
            LogInfoCat(kEditorInputTraceCategory, TEXT("Stopped camera input: A pressed"));
            mEditorMoveInput.bMoveLeft = true;
            LogInfoCat(kEditorInputTraceCategory,
                TEXT("Stopped camera state after A press: W={} A={} S={} D={} Boost={}"),
                mEditorMoveInput.bMoveForward ? 1U : 0U, mEditorMoveInput.bMoveLeft ? 1U : 0U,
                mEditorMoveInput.bMoveBackward ? 1U : 0U, mEditorMoveInput.bMoveRight ? 1U : 0U,
                mEditorMoveInput.bSpeedBoost ? 1U : 0U);
        }
        if (platformInput->WasKeyReleased(Input::EKey::A)) {
            LogInfoCat(kEditorInputTraceCategory, TEXT("Stopped camera input: A released"));
            mEditorMoveInput.bMoveLeft = false;
            LogInfoCat(kEditorInputTraceCategory,
                TEXT("Stopped camera state after A release: W={} A={} S={} D={} Boost={}"),
                mEditorMoveInput.bMoveForward ? 1U : 0U, mEditorMoveInput.bMoveLeft ? 1U : 0U,
                mEditorMoveInput.bMoveBackward ? 1U : 0U, mEditorMoveInput.bMoveRight ? 1U : 0U,
                mEditorMoveInput.bSpeedBoost ? 1U : 0U);
        }
        if (platformInput->WasKeyPressed(Input::EKey::S)) {
            LogInfoCat(kEditorInputTraceCategory, TEXT("Stopped camera input: S pressed"));
            mEditorMoveInput.bMoveBackward = true;
            LogInfoCat(kEditorInputTraceCategory,
                TEXT("Stopped camera state after S press: W={} A={} S={} D={} Boost={}"),
                mEditorMoveInput.bMoveForward ? 1U : 0U, mEditorMoveInput.bMoveLeft ? 1U : 0U,
                mEditorMoveInput.bMoveBackward ? 1U : 0U, mEditorMoveInput.bMoveRight ? 1U : 0U,
                mEditorMoveInput.bSpeedBoost ? 1U : 0U);
        }
        if (platformInput->WasKeyReleased(Input::EKey::S)) {
            LogInfoCat(kEditorInputTraceCategory, TEXT("Stopped camera input: S released"));
            mEditorMoveInput.bMoveBackward = false;
            LogInfoCat(kEditorInputTraceCategory,
                TEXT("Stopped camera state after S release: W={} A={} S={} D={} Boost={}"),
                mEditorMoveInput.bMoveForward ? 1U : 0U, mEditorMoveInput.bMoveLeft ? 1U : 0U,
                mEditorMoveInput.bMoveBackward ? 1U : 0U, mEditorMoveInput.bMoveRight ? 1U : 0U,
                mEditorMoveInput.bSpeedBoost ? 1U : 0U);
        }
        if (platformInput->WasKeyPressed(Input::EKey::D)) {
            LogInfoCat(kEditorInputTraceCategory, TEXT("Stopped camera input: D pressed"));
            mEditorMoveInput.bMoveRight = true;
            LogInfoCat(kEditorInputTraceCategory,
                TEXT("Stopped camera state after D press: W={} A={} S={} D={} Boost={}"),
                mEditorMoveInput.bMoveForward ? 1U : 0U, mEditorMoveInput.bMoveLeft ? 1U : 0U,
                mEditorMoveInput.bMoveBackward ? 1U : 0U, mEditorMoveInput.bMoveRight ? 1U : 0U,
                mEditorMoveInput.bSpeedBoost ? 1U : 0U);
        }
        if (platformInput->WasKeyReleased(Input::EKey::D)) {
            LogInfoCat(kEditorInputTraceCategory, TEXT("Stopped camera input: D released"));
            mEditorMoveInput.bMoveRight = false;
            LogInfoCat(kEditorInputTraceCategory,
                TEXT("Stopped camera state after D release: W={} A={} S={} D={} Boost={}"),
                mEditorMoveInput.bMoveForward ? 1U : 0U, mEditorMoveInput.bMoveLeft ? 1U : 0U,
                mEditorMoveInput.bMoveBackward ? 1U : 0U, mEditorMoveInput.bMoveRight ? 1U : 0U,
                mEditorMoveInput.bSpeedBoost ? 1U : 0U);
        }
        if (platformInput->WasKeyPressed(Input::EKey::LeftShift)
            || platformInput->WasKeyPressed(Input::EKey::RightShift)) {
            mEditorMoveInput.bSpeedBoost = true;
        }
        if (platformInput->WasKeyReleased(Input::EKey::LeftShift)
            || platformInput->WasKeyReleased(Input::EKey::RightShift)) {
            mEditorMoveInput.bSpeedBoost = false;
        }

        const bool isViewportInteractive = IsViewportInteractive(viewport);
        const bool wantsFreelook = isViewportInteractive && mEditorMoveInput.bRightMouseHeld;
        if (mEditorMoveInput.bMoveForward || mEditorMoveInput.bMoveLeft
            || mEditorMoveInput.bMoveBackward || mEditorMoveInput.bMoveRight) {
            LogInfoCat(kEditorInputTraceCategory,
                TEXT(
                    "Stopped camera pre-freelook: wants={} hovered={} focused={} blocked={} rmb={} W={} A={} S={} D={}"),
                wantsFreelook ? 1U : 0U, viewport.bHovered ? 1U : 0U, viewport.bFocused ? 1U : 0U,
                viewport.bUiBlockingInput ? 1U : 0U, mEditorMoveInput.bRightMouseHeld ? 1U : 0U,
                mEditorMoveInput.bMoveForward ? 1U : 0U, mEditorMoveInput.bMoveLeft ? 1U : 0U,
                mEditorMoveInput.bMoveBackward ? 1U : 0U, mEditorMoveInput.bMoveRight ? 1U : 0U);
        }
        if (!isViewportInteractive) {
            RestoreCursorCapture(mainWindow);
            return;
        }

        if (wantsFreelook) {
            if (!mEditorCamera.bCursorCaptured) {
                mainWindow->SetCursorVisible(false);
                mainWindow->SetCursorClippedToClient(true);
                mEditorCamera.bCursorCaptured = true;
            }
            mEditorCamera.bFreelookActive = true;

            mEditorCamera.mRotation.yaw +=
                static_cast<f32>(platformInput->GetMouseDeltaX()) * kEditorCameraLookSpeed;
            mEditorCamera.mRotation.pitch             = Clamp(mEditorCamera.mRotation.pitch
                                - static_cast<f32>(platformInput->GetMouseDeltaY()) * kEditorCameraLookSpeed,
                            -kEditorCameraPitchLimit, kEditorCameraPitchLimit);
            mEditorCamera.mCamera.mTransform.Rotation = mEditorCamera.mRotation.ToQuaternion();
        } else {
            RestoreCursorCapture(mainWindow);
        }

        f32        moveForward = 0.0f;
        f32        moveRight   = 0.0f;
        const bool wDown       = mEditorMoveInput.bMoveForward;
        const bool aDown       = mEditorMoveInput.bMoveLeft;
        const bool sDown       = mEditorMoveInput.bMoveBackward;
        const bool dDown       = mEditorMoveInput.bMoveRight;
        if (wDown) {
            moveForward += 1.0f;
        }
        if (sDown) {
            moveForward -= 1.0f;
        }
        if (dDown) {
            moveRight += 1.0f;
        }
        if (aDown) {
            moveRight -= 1.0f;
        }

        if (wDown || aDown || sDown || dDown) {
            const auto& translation = mEditorCamera.mCamera.mTransform.Translation;
            LogInfoCat(kEditorInputTraceCategory,
                TEXT("Stopped camera hold: W={} A={} S={} D={} pos=({:.3f}, {:.3f}, {:.3f})"),
                wDown ? 1U : 0U, aDown ? 1U : 0U, sDown ? 1U : 0U, dDown ? 1U : 0U, translation[0],
                translation[1], translation[2]);
        }

        if (moveForward != 0.0f || moveRight != 0.0f) {
            const FVector3f forward =
                mEditorCamera.mCamera.mTransform.Rotation.RotateVector(FVector3f(0.0f, 0.0f, 1.0f));
            const FVector3f right =
                mEditorCamera.mCamera.mTransform.Rotation.RotateVector(FVector3f(1.0f, 0.0f, 0.0f));
            auto moveDirection = forward * FVector3f(moveForward) + right * FVector3f(moveRight);
            const f32 moveDirectionLengthSquared = moveDirection[0] * moveDirection[0]
                + moveDirection[1] * moveDirection[1] + moveDirection[2] * moveDirection[2];
            if (moveDirectionLengthSquared <= 1e-8f) {
                return;
            }
            const f32 inverseLength = 1.0f / Core::Math::Sqrt(moveDirectionLengthSquared);
            moveDirection           = FVector3f(moveDirection[0] * inverseLength,
                          moveDirection[1] * inverseLength, moveDirection[2] * inverseLength);
            f32 moveSpeed           = kEditorCameraMoveSpeed;
            if (mEditorMoveInput.bSpeedBoost) {
                moveSpeed *= kEditorCameraBoostScale;
            }
            const f32 moveScale = moveSpeed * frameContext.DeltaSeconds;
            mEditorCamera.mCamera.mTransform.Translation += moveDirection * FVector3f(moveScale);
            const auto& translation = mEditorCamera.mCamera.mTransform.Translation;
            LogInfoCat(kEditorInputTraceCategory,
                TEXT(
                    "Stopped camera moved: deltaScale={:.3f} dir=({:.3f}, {:.3f}, {:.3f}) pos=({:.3f}, {:.3f}, {:.3f})"),
                moveScale, moveDirection[0], moveDirection[1], moveDirection[2], translation[0],
                translation[1], translation[2]);
        }
    }

    void FEditorPlaySession::OnFrameConsumed() { mController.OnFrameConsumed(); }

    void FEditorPlaySession::Start(Launch::IRuntimeSession& session) {
        mController.OnSessionShutdown();
        auto services = session.GetServices();
        if (services.WorldManager != nullptr) {
            SeedEditorCameraFromWorld(services.WorldManager->GetActiveWorld());
        }
    }

    void FEditorPlaySession::Shutdown(Application::FPlatformWindow* mainWindow) {
        RestoreCursorCapture(mainWindow);
        mPlayWorldState = {};
        mEditorCamera   = {};
        mController.OnSessionShutdown();
    }

    auto FEditorPlaySession::GetState() const -> Launch::EEditorRuntimeState {
        return mController.GetState();
    }

    auto FEditorPlaySession::IsInspectorReadOnly() const -> bool {
        return GetState() == Launch::EEditorRuntimeState::Running;
    }

    void FEditorPlaySession::SeedEditorCameraFromWorld(const GameScene::FWorld* world) {
        RenderCore::View::FCameraData camera{};
        camera.mProjectionType     = ECameraProjectionType::Perspective;
        camera.mVerticalFovRadians = Core::Math::kPiF / 3.0f;
        camera.mNearPlane          = 0.1f;
        camera.mFarPlane           = 1000.0f;
        camera.mTransform          = Core::Math::LinAlg::FSpatialTransform::Identity();

        if (world != nullptr) {
            const auto& cameraIds = world->GetActiveCameraComponents();
            for (const auto& componentId : cameraIds) {
                if (!world->IsAlive(componentId)) {
                    continue;
                }

                const auto& component =
                    world->ResolveComponent<GameScene::FCameraComponent>(componentId);
                if (!component.IsEnabled() || !world->IsGameObjectActive(component.GetOwner())) {
                    continue;
                }

                camera.mVerticalFovRadians = component.GetFovYRadians();
                camera.mNearPlane          = component.GetNearPlane();
                camera.mFarPlane           = component.GetFarPlane();
                camera.mTransform = world->Object(component.GetOwner()).GetWorldTransform();
                break;
            }
        }

        mEditorCamera.mCamera      = camera;
        mEditorCamera.mRotation    = FEulerRotator(camera.mTransform.Rotation);
        mEditorCamera.bInitialized = true;
    }

    void FEditorPlaySession::CaptureEditorCameraForPlay() {
        mPlayWorldState.mSavedEditorCamera = mEditorCamera;
    }

    void FEditorPlaySession::RestoreCursorCapture(Application::FPlatformWindow* mainWindow) {
        if (mainWindow != nullptr && mEditorCamera.bCursorCaptured) {
            mainWindow->SetCursorClippedToClient(false);
            mainWindow->SetCursorVisible(true);
        }
        mEditorCamera.bCursorCaptured = false;
        mEditorCamera.bFreelookActive = false;
    }
} // namespace AltinaEngine::Editor::PlaySession
