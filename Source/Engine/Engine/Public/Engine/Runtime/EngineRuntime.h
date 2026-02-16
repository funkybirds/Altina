#pragma once

#include "Engine/EngineAPI.h"
#include "Engine/GameScene/WorldManager.h"

namespace AltinaEngine::Engine {
    class AE_ENGINE_API FEngineRuntime {
    public:
        FEngineRuntime()  = default;
        ~FEngineRuntime() = default;

        FEngineRuntime(const FEngineRuntime&)                    = delete;
        auto operator=(const FEngineRuntime&) -> FEngineRuntime& = delete;
        FEngineRuntime(FEngineRuntime&&)                         = delete;
        auto operator=(FEngineRuntime&&) -> FEngineRuntime&      = delete;

        [[nodiscard]] auto GetWorldManager() noexcept -> GameScene::FWorldManager&;
        [[nodiscard]] auto GetWorldManager() const noexcept
            -> const GameScene::FWorldManager&;

    private:
        GameScene::FWorldManager mWorldManager{};
    };
} // namespace AltinaEngine::Engine
