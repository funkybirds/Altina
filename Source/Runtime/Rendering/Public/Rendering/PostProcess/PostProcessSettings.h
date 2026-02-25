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
    AE_RENDERING_API extern Core::Console::TConsoleVariable<i32> rPostProcessTaa;

    // Bloom tuning (used as defaults for the built-in Bloom node).
    AE_RENDERING_API extern Core::Console::TConsoleVariable<f32> rPostProcessBloomThreshold;
    AE_RENDERING_API extern Core::Console::TConsoleVariable<f32> rPostProcessBloomKnee;
    AE_RENDERING_API extern Core::Console::TConsoleVariable<f32> rPostProcessBloomIntensity;
    AE_RENDERING_API extern Core::Console::TConsoleVariable<f32> rPostProcessBloomKawaseOffset;
    AE_RENDERING_API extern Core::Console::TConsoleVariable<i32> rPostProcessBloomIterations;

    // FXAA tuning (used as defaults for the built-in FXAA node).
    AE_RENDERING_API extern Core::Console::TConsoleVariable<f32> rPostProcessFxaaEdgeThreshold;
    AE_RENDERING_API extern Core::Console::TConsoleVariable<f32> rPostProcessFxaaEdgeThresholdMin;
    AE_RENDERING_API extern Core::Console::TConsoleVariable<f32> rPostProcessFxaaSubpix;

    // TAA tuning (used as defaults for the built-in TAA node).
    AE_RENDERING_API extern Core::Console::TConsoleVariable<f32> rPostProcessTaaAlpha;
    AE_RENDERING_API extern Core::Console::TConsoleVariable<f32> rPostProcessTaaClampK;

    // Temporal jitter controls (used by temporal AA view-state preparation).
    AE_RENDERING_API extern Core::Console::TConsoleVariable<i32> rTemporalJitter;
    AE_RENDERING_API extern Core::Console::TConsoleVariable<i32> rTemporalJitterSampleCount;
} // namespace AltinaEngine::Rendering
