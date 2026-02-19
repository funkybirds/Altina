#pragma once

#include "Rendering/RenderingAPI.h"
#include "Console/ConsoleVariable.h"
#include "Types/Aliases.h"

namespace AltinaEngine::Rendering {
    enum class ERendererType : i32 {
        Forward  = 0,
        Deferred = 1
    };

    AE_RENDERING_API extern Core::Console::TConsoleVariable<i32> gRendererType;

    [[nodiscard]] inline auto GetRendererTypeSetting() noexcept -> ERendererType {
        const i32 value = gRendererType.Get();
        if (value <= 0) {
            return ERendererType::Forward;
        }
        return ERendererType::Deferred;
    }
} // namespace AltinaEngine::Rendering
