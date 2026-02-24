#include "Rendering/PostProcess/PostProcessSettings.h"

namespace AltinaEngine::Rendering {
    Core::Console::TConsoleVariable<i32> rPostProcessEnable(TEXT("r.PostProcess.Enable"), 1);
    Core::Console::TConsoleVariable<i32> rPostProcessTonemap(TEXT("r.PostProcess.Tonemap"), 1);
    Core::Console::TConsoleVariable<i32> rPostProcessBloom(TEXT("r.PostProcess.Bloom"), 0);
    Core::Console::TConsoleVariable<i32> rPostProcessFxaa(TEXT("r.PostProcess.Fxaa"), 0);

    Core::Console::TConsoleVariable<f32> rPostProcessBloomThreshold(
        TEXT("r.PostProcess.Bloom.Threshold"), 1.0f);
    Core::Console::TConsoleVariable<f32> rPostProcessBloomKnee(
        TEXT("r.PostProcess.Bloom.Knee"), 0.5f);
    Core::Console::TConsoleVariable<f32> rPostProcessBloomIntensity(
        TEXT("r.PostProcess.Bloom.Intensity"), 0.05f);
    Core::Console::TConsoleVariable<f32> rPostProcessBloomKawaseOffset(
        TEXT("r.PostProcess.Bloom.KawaseOffset"), 1.0f);
    Core::Console::TConsoleVariable<i32> rPostProcessBloomIterations(
        TEXT("r.PostProcess.Bloom.Iterations"), 5);

    Core::Console::TConsoleVariable<f32> rPostProcessFxaaEdgeThreshold(
        TEXT("r.PostProcess.Fxaa.EdgeThreshold"), 0.125f);
    Core::Console::TConsoleVariable<f32> rPostProcessFxaaEdgeThresholdMin(
        TEXT("r.PostProcess.Fxaa.EdgeThresholdMin"), 0.0312f);
    Core::Console::TConsoleVariable<f32> rPostProcessFxaaSubpix(
        TEXT("r.PostProcess.Fxaa.Subpix"), 0.75f);
} // namespace AltinaEngine::Rendering
