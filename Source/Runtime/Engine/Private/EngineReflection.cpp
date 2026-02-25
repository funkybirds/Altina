#include "Engine/EngineReflection.h"

namespace AltinaEngine::Core::Reflection {
    void RegisterReflection_AltinaEngineEngine();
}

namespace AltinaEngine::Engine {
    void RegisterEngineReflection() {
        static bool gRegistered = false;
        if (gRegistered) {
            return;
        }
        gRegistered = true;
        Core::Reflection::RegisterReflection_AltinaEngineEngine();
    }
} // namespace AltinaEngine::Engine
