#include "Launch/GameClient.h"

namespace AltinaEngine::Launch {
    auto RunGameClient(FGameClient& client, const FStartupParameters& startupParameters) -> int {
        FEngineLoop engineLoop(startupParameters);
        if (!engineLoop.PreInit()) {
            return 1;
        }

        if (!client.OnPreInit(engineLoop)) {
            engineLoop.Exit();
            return 1;
        }

        if (!engineLoop.Init()) {
            engineLoop.Exit();
            return 1;
        }

        if (!client.OnInit(engineLoop)) {
            engineLoop.Exit();
            return 1;
        }

        const float fixedDeltaSeconds = client.GetFixedDeltaTimeSeconds();
        while (client.OnTick(engineLoop, fixedDeltaSeconds)) {
        }

        client.OnShutdown(engineLoop);
        engineLoop.Exit();
        return 0;
    }
} // namespace AltinaEngine::Launch
