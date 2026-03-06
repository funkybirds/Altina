#include "EditorPlaySession/EditorPlaySession.h"

#include "Input/InputSystem.h"
#include "Input/Keys.h"
#include "Launch/RuntimeSession.h"

namespace AltinaEngine::Editor::PlaySession {
    void FEditorPlaySession::HandleFrameInput(const Input::FInputSystem* inputSystem) {
        if (inputSystem == nullptr) {
            return;
        }

        if (inputSystem->WasKeyPressed(Input::EKey::F5)) {
            mController.RequestPlay();
        }
        if (inputSystem->WasKeyPressed(Input::EKey::F6)) {
            mController.RequestPause();
        }
        if (inputSystem->WasKeyPressed(Input::EKey::F10)) {
            mController.RequestStep();
        }
        if (inputSystem->WasKeyPressed(Input::EKey::F8)) {
            mController.RequestStop();
        }
    }

    auto FEditorPlaySession::ShouldTickSimulation() const -> bool {
        return mController.ShouldTickSimulation();
    }

    auto FEditorPlaySession::BuildSimulationTick(const Launch::FFrameContext& frameContext) const
        -> Launch::FSimulationTick {
        return mController.BuildSimulationTick(frameContext.DeltaSeconds);
    }

    void FEditorPlaySession::OnFrameConsumed() { mController.OnFrameConsumed(); }

    void FEditorPlaySession::Start() {
        mController.RequestPlay();
        mController.OnSessionInitialized();
    }

    void FEditorPlaySession::Shutdown() { mController.OnSessionShutdown(); }

    auto FEditorPlaySession::GetState() const -> Launch::EEditorRuntimeState {
        return mController.GetState();
    }
} // namespace AltinaEngine::Editor::PlaySession
