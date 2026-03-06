#include "Launch/RuntimeSession.h"

#include "Launch/EngineLoop.h"
#include "Container/SmartPtr.h"
namespace AltinaEngine::Launch {
    auto CreateDefaultRuntimeSession(const FStartupParameters& startupParameters)
        -> FRuntimeSessionOwner {
        return AltinaEngine::Core::Container::MakeUniqueAs<IRuntimeSession, FEngineLoop>(
            startupParameters);
    }
} // namespace AltinaEngine::Launch
