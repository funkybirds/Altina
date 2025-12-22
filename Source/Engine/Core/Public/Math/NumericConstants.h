#pragma once

#include "Types/Aliases.h"

namespace AltinaEngine::Core::Math
{

    struct FNumericConstants
    {
        // Dynamic extent marker used by span-like containers.
        static constexpr AltinaEngine::usize kDynamicSized = static_cast<AltinaEngine::usize>(-1);
    };

} // namespace AltinaEngine::Core::Math
