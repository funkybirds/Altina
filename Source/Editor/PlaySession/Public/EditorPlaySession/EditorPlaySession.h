#pragma once

#include "Base/EditorPlaySessionAPI.h"
#include "Launch/EditorRuntimeController.h"

namespace AltinaEngine::Input {
    class FInputSystem;
}

namespace AltinaEngine::Launch {
    struct FFrameContext;
}

namespace AltinaEngine::Editor::PlaySession {
    class AE_EDITOR_PLAYSESSION_API FEditorPlaySession final {
    public:
        void HandleFrameInput(const Input::FInputSystem* inputSystem, bool allowHotkeys = true);
        void RequestPlay();
        void RequestPause();
        void RequestStep();
        void RequestStop();
        [[nodiscard]] auto ShouldTickSimulation() const -> bool;
        [[nodiscard]] auto BuildSimulationTick(const Launch::FFrameContext& frameContext) const
            -> Launch::FSimulationTick;
        void               OnFrameConsumed();
        void               Start();
        void               Shutdown();
        [[nodiscard]] auto GetState() const -> Launch::EEditorRuntimeState;

    private:
        Launch::FEditorRuntimeController mController{};
    };
} // namespace AltinaEngine::Editor::PlaySession
