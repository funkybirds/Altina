#pragma once

#include "Rendering/RenderingAPI.h"

#include "Console/ConsoleVariable.h"
#include "Types/Aliases.h"

namespace AltinaEngine::Rendering {
    // Global debug overrides / defaults for the built-in post-process stack.
    AE_RENDERING_API extern Core::Console::TConsoleVariable<i32> rPostProcessEnable;
    AE_RENDERING_API extern Core::Console::TConsoleVariable<i32> rPostProcessTonemap;
    AE_RENDERING_API extern Core::Console::TConsoleVariable<i32> rPostProcessBloom;
    AE_RENDERING_API extern Core::Console::TConsoleVariable<i32> rPostProcessFxaa;
} // namespace AltinaEngine::Rendering
