#include "Rendering/PostProcess/PostProcessSettings.h"

namespace AltinaEngine::Rendering {
    Core::Console::TConsoleVariable<i32> rPostProcessEnable(TEXT("r.PostProcess.Enable"), 1);
    Core::Console::TConsoleVariable<i32> rPostProcessTonemap(TEXT("r.PostProcess.Tonemap"), 1);
    Core::Console::TConsoleVariable<i32> rPostProcessBloom(TEXT("r.PostProcess.Bloom"), 0);
    Core::Console::TConsoleVariable<i32> rPostProcessFxaa(TEXT("r.PostProcess.Fxaa"), 0);
} // namespace AltinaEngine::Rendering
