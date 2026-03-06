#pragma once

#include "Base/LaunchAPI.h"
#include "Launch/RuntimeSession.h"
#include "Math/Common.h"

namespace AltinaEngine::Launch {
    enum class EEditorRuntimeState : u8 {
        Stopped = 0,
        Starting,
        Running,
        Paused,
        Stepping,
        Stopping
    };

    class AE_LAUNCH_API FEditorRuntimeController final {
    public:
        void               RequestPlay() noexcept;
        void               RequestPause() noexcept;
        void               RequestStep() noexcept;
        void               RequestStop() noexcept;
        void               SetTimeScale(f32 timeScale) noexcept;

        void               OnSessionInitialized() noexcept;
        void               OnSessionShutdown() noexcept;
        void               OnFrameConsumed() noexcept;

        [[nodiscard]] auto ShouldTickSimulation() const noexcept -> bool;
        [[nodiscard]] auto BuildSimulationTick(f32 fixedDeltaSeconds) const noexcept
            -> FSimulationTick;
        [[nodiscard]] auto GetState() const noexcept -> EEditorRuntimeState { return mState; }
        [[nodiscard]] auto GetTimeScale() const noexcept -> f32 { return mTimeScale; }

    private:
        EEditorRuntimeState mState     = EEditorRuntimeState::Stopped;
        f32                 mTimeScale = 1.0f;
    };
} // namespace AltinaEngine::Launch
