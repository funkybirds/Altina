#pragma once

#include "Base/LaunchAPI.h"
#include "CoreMinimal.h"
#include "Launch/EngineLoop.h"

namespace AltinaEngine::Launch {
    class AE_LAUNCH_API FGameClient {
    public:
        virtual ~FGameClient() = default;

        virtual auto OnPreInit(FEngineLoop& engineLoop) -> bool { return true; }
        virtual auto OnInit(FEngineLoop& engineLoop) -> bool { return true; }
        virtual auto OnTick(FEngineLoop& engineLoop, float deltaSeconds) -> bool { return true; }
        virtual void OnShutdown(FEngineLoop& engineLoop) {}
        [[nodiscard]] virtual auto GetFixedDeltaTimeSeconds() const -> float { return 1.0f / 60.0f; }
    };

    AE_LAUNCH_API auto RunGameClient(FGameClient& client, const FStartupParameters& startupParameters) -> int;
} // namespace AltinaEngine::Launch
