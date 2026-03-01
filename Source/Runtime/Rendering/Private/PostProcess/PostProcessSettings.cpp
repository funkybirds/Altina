#include "Rendering/PostProcess/PostProcessSettings.h"

using AltinaEngine::Core::Console::ECVarFlags;
using AltinaEngine::Core::Console::TConsoleVariable;

namespace AltinaEngine::Rendering {
    TConsoleVariable<i32> rPostProcessEnable(
        TEXT("r.PostProcess.Enable"), 1, ECVarFlags::SnapshotPerFrame);
    TConsoleVariable<i32> rPostProcessTonemap(
        TEXT("r.PostProcess.Tonemap"), 1, ECVarFlags::SnapshotPerFrame);
    TConsoleVariable<i32> rPostProcessBloom(
        TEXT("r.PostProcess.Bloom"), 0, ECVarFlags::SnapshotPerFrame);
    TConsoleVariable<i32> rPostProcessFxaa(
        TEXT("r.PostProcess.Fxaa"), 0, ECVarFlags::SnapshotPerFrame);
    TConsoleVariable<i32> rPostProcessTaa(
        TEXT("r.PostProcess.Taa"), 0, ECVarFlags::SnapshotPerFrame);

    TConsoleVariable<f32> rPostProcessBloomThreshold(
        TEXT("r.PostProcess.Bloom.Threshold"), 1.0f, ECVarFlags::SnapshotPerFrame);
    TConsoleVariable<f32> rPostProcessBloomKnee(
        TEXT("r.PostProcess.Bloom.Knee"), 0.5f, ECVarFlags::SnapshotPerFrame);
    TConsoleVariable<f32> rPostProcessBloomIntensity(
        TEXT("r.PostProcess.Bloom.Intensity"), 0.05f, ECVarFlags::SnapshotPerFrame);
    TConsoleVariable<f32> rPostProcessBloomKawaseOffset(
        TEXT("r.PostProcess.Bloom.KawaseOffset"), 1.0f, ECVarFlags::SnapshotPerFrame);
    TConsoleVariable<i32> rPostProcessBloomIterations(
        TEXT("r.PostProcess.Bloom.Iterations"), 5, ECVarFlags::SnapshotPerFrame);
    TConsoleVariable<i32> rPostProcessBloomFirstDownsampleLumaWeight(
        TEXT("r.PostProcess.Bloom.FirstDownsampleLumaWeight"), 1, ECVarFlags::SnapshotPerFrame);

    TConsoleVariable<f32> rPostProcessFxaaEdgeThreshold(
        TEXT("r.PostProcess.Fxaa.EdgeThreshold"), 0.125f, ECVarFlags::SnapshotPerFrame);
    TConsoleVariable<f32> rPostProcessFxaaEdgeThresholdMin(
        TEXT("r.PostProcess.Fxaa.EdgeThresholdMin"), 0.0312f, ECVarFlags::SnapshotPerFrame);
    TConsoleVariable<f32> rPostProcessFxaaSubpix(
        TEXT("r.PostProcess.Fxaa.Subpix"), 0.75f, ECVarFlags::SnapshotPerFrame);

    TConsoleVariable<f32> rPostProcessTaaAlpha(
        TEXT("r.PostProcess.Taa.Alpha"), 0.9f, ECVarFlags::SnapshotPerFrame);
    TConsoleVariable<f32> rPostProcessTaaClampK(
        TEXT("r.PostProcess.Taa.ClampK"), 1.0f, ECVarFlags::SnapshotPerFrame);

    TConsoleVariable<i32> rTemporalJitter(
        TEXT("r.TemporalJitter"), 0, ECVarFlags::SnapshotPerFrame);
    TConsoleVariable<i32> rTemporalJitterSampleCount(
        TEXT("r.TemporalJitter.SampleCount"), 8, ECVarFlags::SnapshotPerFrame);
} // namespace AltinaEngine::Rendering
