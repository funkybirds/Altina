#include "Rendering/PostProcess/PostProcessSettings.h"

using AltinaEngine::Core::Console::TConsoleVariable;

namespace AltinaEngine::Rendering {
    TConsoleVariable<i32> rPostProcessEnable(TEXT("r.PostProcess.Enable"), 1);
    TConsoleVariable<i32> rPostProcessTonemap(TEXT("r.PostProcess.Tonemap"), 1);
    TConsoleVariable<i32> rPostProcessBloom(TEXT("r.PostProcess.Bloom"), 0);
    TConsoleVariable<i32> rPostProcessFxaa(TEXT("r.PostProcess.Fxaa"), 0);
    TConsoleVariable<i32> rPostProcessTaa(TEXT("r.PostProcess.Taa"), 0);

    TConsoleVariable<f32> rPostProcessBloomThreshold(TEXT("r.PostProcess.Bloom.Threshold"), 1.0f);
    TConsoleVariable<f32> rPostProcessBloomKnee(TEXT("r.PostProcess.Bloom.Knee"), 0.5f);
    TConsoleVariable<f32> rPostProcessBloomIntensity(TEXT("r.PostProcess.Bloom.Intensity"), 0.05f);
    TConsoleVariable<f32> rPostProcessBloomKawaseOffset(
        TEXT("r.PostProcess.Bloom.KawaseOffset"), 1.0f);
    TConsoleVariable<i32> rPostProcessBloomIterations(TEXT("r.PostProcess.Bloom.Iterations"), 5);

    TConsoleVariable<f32> rPostProcessFxaaEdgeThreshold(
        TEXT("r.PostProcess.Fxaa.EdgeThreshold"), 0.125f);
    TConsoleVariable<f32> rPostProcessFxaaEdgeThresholdMin(
        TEXT("r.PostProcess.Fxaa.EdgeThresholdMin"), 0.0312f);
    TConsoleVariable<f32> rPostProcessFxaaSubpix(TEXT("r.PostProcess.Fxaa.Subpix"), 0.75f);

    TConsoleVariable<f32> rPostProcessTaaAlpha(TEXT("r.PostProcess.Taa.Alpha"), 0.9f);
    TConsoleVariable<f32> rPostProcessTaaClampK(TEXT("r.PostProcess.Taa.ClampK"), 1.0f);

    TConsoleVariable<i32> rTemporalJitter(TEXT("r.TemporalJitter"), 0);
    TConsoleVariable<i32> rTemporalJitterSampleCount(TEXT("r.TemporalJitter.SampleCount"), 8);
} // namespace AltinaEngine::Rendering
