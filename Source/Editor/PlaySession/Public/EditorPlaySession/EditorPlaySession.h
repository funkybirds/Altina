#pragma once

#include "Base/EditorPlaySessionAPI.h"
#include "Container/String.h"
#include "Engine/GameScene/WorldManager.h"
#include "Launch/EditorRuntimeController.h"
#include "Launch/RuntimeSession.h"
#include "Math/Rotation.h"
#include "View/CameraData.h"

namespace AltinaEngine::Input {
    class FInputSystem;
}

namespace AltinaEngine::Application {
    class FPlatformWindow;
}

namespace AltinaEngine::Launch {
    struct FFrameContext;
}

namespace AltinaEngine::Editor::PlaySession {
    struct FEditorViewportInteraction {
        u32  mWidth           = 0U;
        u32  mHeight          = 0U;
        i32  mContentMinX     = 0;
        i32  mContentMinY     = 0;
        bool bHovered         = false;
        bool bFocused         = false;
        bool bHasContent      = false;
        bool bUiBlockingInput = false;
    };

    class AE_EDITOR_PLAYSESSION_API FEditorPlaySession final {
    public:
        void HandleFrameInput(const Input::FInputSystem* inputSystem, bool allowHotkeys = true);
        auto RequestPlay(Launch::IRuntimeSession& session) -> bool;
        void RequestPause();
        void RequestStep();
        auto RequestStop(Launch::IRuntimeSession& session) -> bool;
        [[nodiscard]] auto ShouldTickSimulation() const -> bool;
        [[nodiscard]] auto BuildSimulationTick(const Launch::FFrameContext& frameContext) const
            -> Launch::FSimulationTick;
        [[nodiscard]] auto BuildRenderTick(const FEditorViewportInteraction& viewport) const
            -> Launch::FRenderTick;
        void               UpdateEditorCamera(const Input::FInputSystem* platformInput,
                          Application::FPlatformWindow* mainWindow, const FEditorViewportInteraction& viewport,
                          const Launch::FFrameContext& frameContext);
        void               OnFrameConsumed();
        void               Start(Launch::IRuntimeSession& session);
        void               Shutdown(Application::FPlatformWindow* mainWindow = nullptr);
        [[nodiscard]] auto GetState() const -> Launch::EEditorRuntimeState;
        [[nodiscard]] auto IsInspectorReadOnly() const -> bool;

    private:
        struct FEditorViewportCameraState {
            RenderCore::View::FCameraData             mCamera{};
            ::AltinaEngine::Core::Math::FEulerRotator mRotation{};
            bool                                      bInitialized    = false;
            bool                                      bFreelookActive = false;
            bool                                      bCursorCaptured = false;
        };

        struct FEditorViewportMoveInputState {
            bool bMoveForward    = false;
            bool bMoveLeft       = false;
            bool bMoveBackward   = false;
            bool bMoveRight      = false;
            bool bSpeedBoost     = false;
            bool bRightMouseHeld = false;
        };

        struct FEditorPlayWorldState {
            ::AltinaEngine::Core::Container::FNativeString mWorldSnapshot{};
            GameScene::FWorldHandle                        mEditorWorldHandle{};
            GameScene::FWorldHandle                        mRuntimeWorldHandle{};
            FEditorViewportCameraState                     mSavedEditorCamera{};
            bool                                           bHasSnapshot = false;
        };

        void SeedEditorCameraFromWorld(const GameScene::FWorld* world);
        void CaptureEditorCameraForPlay();
        void RestoreCursorCapture(Application::FPlatformWindow* mainWindow);

        Launch::FEditorRuntimeController mController{};
        FEditorViewportCameraState       mEditorCamera{};
        FEditorViewportMoveInputState    mEditorMoveInput{};
        FEditorPlayWorldState            mPlayWorldState{};
    };
} // namespace AltinaEngine::Editor::PlaySession
