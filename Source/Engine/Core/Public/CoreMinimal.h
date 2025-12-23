#pragma once

#include "Base/CoreAPI.h"
#include "Logging/Log.h"
#include "Math/Vector.h"
#include "Types/Aliases.h"
#include "Types/Concepts.h"

#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace AltinaEngine
{

    struct FStartupParameters
    {
        std::string CommandLine; // Raw command-line passed to the engine.
    };

} // namespace AltinaEngine
