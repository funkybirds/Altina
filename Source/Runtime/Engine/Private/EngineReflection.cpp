#include "Engine/EngineReflection.h"

namespace AltinaEngine::Core::Reflection {
    void RegisterReflection_AltinaEngineEngine();
}

namespace AltinaEngine::Engine {
    void RegisterEngineReflection() { Core::Reflection::RegisterReflection_AltinaEngineEngine(); }
} // namespace AltinaEngine::Engine
