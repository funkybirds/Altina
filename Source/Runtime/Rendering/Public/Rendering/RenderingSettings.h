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

    // Image-based lighting (IBL) for deferred lighting (diffuse irradiance + specular prefilter).
    AE_RENDERING_API extern Core::Console::TConsoleVariable<i32> rIblEnable;
    AE_RENDERING_API extern Core::Console::TConsoleVariable<f32> rIblDiffuseIntensity;
    AE_RENDERING_API extern Core::Console::TConsoleVariable<f32> rIblSpecularIntensity;
    AE_RENDERING_API extern Core::Console::TConsoleVariable<f32> rIblSaturation;

    // SSAO (screen-space ambient occlusion) for the deferred renderer.
    AE_RENDERING_API extern Core::Console::TConsoleVariable<i32> rSsaoEnable;
    AE_RENDERING_API extern Core::Console::TConsoleVariable<i32> rSsaoSampleCount;
    AE_RENDERING_API extern Core::Console::TConsoleVariable<f32> rSsaoRadiusVS;
    AE_RENDERING_API extern Core::Console::TConsoleVariable<f32> rSsaoBiasNdc;
    AE_RENDERING_API extern Core::Console::TConsoleVariable<f32> rSsaoPower;
    AE_RENDERING_API extern Core::Console::TConsoleVariable<f32> rSsaoIntensity;

    // Vertex layout source switch (0=legacy/factory fixed mapping, 1=shader-reflection path).
    // Reflection mode is scaffold-only for now and remains disabled by default.
    AE_RENDERING_API extern Core::Console::TConsoleVariable<i32> rVertexLayoutUseShaderReflection;

    [[nodiscard]] inline auto GetRendererTypeSetting() noexcept -> ERendererType {
        const i32 value = gRendererType.Get();
        if (value <= 0) {
            return ERendererType::Forward;
        }
        return ERendererType::Deferred;
    }
} // namespace AltinaEngine::Rendering
