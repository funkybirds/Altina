#pragma once

#include "Logging/Log.h"

#include <string>

namespace AltinaEngine {

    struct FStartupParameters {
        std::string mCommandLine; // Raw command-line passed to the engine.
    };

} // namespace AltinaEngine
