#pragma once

#include "Base/CoreAPI.h"
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

    class AE_CORE_API  FApplication;

    struct AE_CORE_API FStartupParameters
    {
        std::string CommandLine; // Raw command-line passed to the engine.
    };

} // namespace AltinaEngine
