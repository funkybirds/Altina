#pragma once

#include "Logging/Log.h"
#include "Container/String.h"

namespace AltinaEngine {

    struct FStartupParameters {
        Core::Container::FNativeString mCommandLine; // Raw command-line passed to the engine.
    };

} // namespace AltinaEngine
