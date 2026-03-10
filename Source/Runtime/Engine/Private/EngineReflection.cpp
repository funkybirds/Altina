#include "Engine/EngineReflection.h"

namespace AltinaEngine::Core::Reflection {
    void RegisterReflection_AltinaEngineEngine(); // NOLINT
}

namespace AltinaEngine::GameScene {
    void RegisterComponent_AltinaEngineEngine();     // NOLINT
    void RegisterComponentJson_AltinaEngineEngine(); // NOLINT
} // namespace AltinaEngine::GameScene

namespace AltinaEngine::Engine {
    void RegisterEngineReflection() {
        static bool gRegistered = false;
        if (gRegistered) {
            return;
        }
        gRegistered = true;
        Core::Reflection::RegisterReflection_AltinaEngineEngine();
        GameScene::RegisterComponent_AltinaEngineEngine();
        GameScene::RegisterComponentJson_AltinaEngineEngine();
    }
} // namespace AltinaEngine::Engine
