#include "Launch/EditorRuntimeController.h"

namespace AltinaEngine::Launch {
    void FEditorRuntimeController::RequestPlay() noexcept {
        if (mState == EEditorRuntimeState::Stopped || mState == EEditorRuntimeState::Starting
            || mState == EEditorRuntimeState::Paused) {
            mState = EEditorRuntimeState::Running;
        }
    }

    void FEditorRuntimeController::RequestPause() noexcept {
        if (mState == EEditorRuntimeState::Running || mState == EEditorRuntimeState::Stepping) {
            mState = EEditorRuntimeState::Paused;
        }
    }

    void FEditorRuntimeController::RequestStep() noexcept {
        if (mState == EEditorRuntimeState::Paused) {
            mState = EEditorRuntimeState::Stepping;
        }
    }

    void FEditorRuntimeController::RequestStop() noexcept {
        if (mState != EEditorRuntimeState::Stopped) {
            mState = EEditorRuntimeState::Stopping;
        }
    }

    void FEditorRuntimeController::SetTimeScale(f32 timeScale) noexcept {
        constexpr f32 kMinScale = 0.01f;
        constexpr f32 kMaxScale = 8.0f;
        mTimeScale              = Core::Math::Clamp(timeScale, kMinScale, kMaxScale);
    }

    void FEditorRuntimeController::OnSessionInitialized() noexcept {
        if (mState == EEditorRuntimeState::Starting || mState == EEditorRuntimeState::Stopped) {
            mState = EEditorRuntimeState::Running;
        }
    }

    void FEditorRuntimeController::OnSessionShutdown() noexcept {
        mState = EEditorRuntimeState::Stopped;
    }

    void FEditorRuntimeController::OnFrameConsumed() noexcept {
        if (mState == EEditorRuntimeState::Stepping) {
            mState = EEditorRuntimeState::Paused;
        } else if (mState == EEditorRuntimeState::Stopping) {
            mState = EEditorRuntimeState::Stopped;
        }
    }

    auto FEditorRuntimeController::ShouldTickSimulation() const noexcept -> bool {
        return mState == EEditorRuntimeState::Running || mState == EEditorRuntimeState::Stepping;
    }

    auto FEditorRuntimeController::BuildSimulationTick(f32 fixedDeltaSeconds) const noexcept
        -> FSimulationTick {
        FSimulationTick tick{};
        tick.DeltaSeconds = fixedDeltaSeconds;
        tick.TimeScale    = mTimeScale;
        tick.bSingleStep  = (mState == EEditorRuntimeState::Stepping);
        return tick;
    }
} // namespace AltinaEngine::Launch
