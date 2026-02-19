#include "Rendering/RenderingSettings.h"

namespace AltinaEngine::Rendering {
    Core::Console::TConsoleVariable<i32> gRendererType(
        TEXT("gRendererType"),
        1);
} // namespace AltinaEngine::Rendering
