#include "Engine/Runtime/EngineRuntime.h"

namespace AltinaEngine::Engine {
    auto FEngineRuntime::GetWorldManager() noexcept -> GameScene::FWorldManager& {
        return mWorldManager;
    }

    auto FEngineRuntime::GetWorldManager() const noexcept -> const GameScene::FWorldManager& {
        return mWorldManager;
    }
} // namespace AltinaEngine::Engine
