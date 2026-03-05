#include "Rendering/RenderingSettings.h"

namespace AltinaEngine::Rendering {
    using Core::Console::ECVarFlags;

    Core::Console::TConsoleVariable<i32> gRendererType(TEXT("rc.RendererType"), 1);

    Core::Console::TConsoleVariable<i32> rIblEnable(
        TEXT("r.Ibl.Enable"), 1, ECVarFlags::SnapshotPerFrame);
    Core::Console::TConsoleVariable<f32> rIblDiffuseIntensity(
        TEXT("r.Ibl.DiffuseIntensity"), 0.8f, ECVarFlags::SnapshotPerFrame);
    Core::Console::TConsoleVariable<f32> rIblSpecularIntensity(
        TEXT("r.Ibl.SpecularIntensity"), 1.0f, ECVarFlags::SnapshotPerFrame);
    Core::Console::TConsoleVariable<f32> rIblSaturation(
        TEXT("r.Ibl.Saturation"), 1.15f, ECVarFlags::SnapshotPerFrame);

    Core::Console::TConsoleVariable<i32> rSsaoEnable(
        TEXT("r.Ssao.Enable"), 1, ECVarFlags::SnapshotPerFrame);
    Core::Console::TConsoleVariable<i32> rSsaoSampleCount(
        TEXT("r.SsaoSampleCount"), 12, ECVarFlags::SnapshotPerFrame);
    Core::Console::TConsoleVariable<f32> rSsaoRadiusVS(
        TEXT("r.Ssao.RadiusVS"), 0.55f, ECVarFlags::SnapshotPerFrame);
    Core::Console::TConsoleVariable<f32> rSsaoBiasNdc(
        TEXT("r.Ssao.BiasNdc"), 0.0005f, ECVarFlags::SnapshotPerFrame);
    Core::Console::TConsoleVariable<f32> rSsaoPower(
        TEXT("r.Ssao.Power"), 1.6f, ECVarFlags::SnapshotPerFrame);
    Core::Console::TConsoleVariable<f32> rSsaoIntensity(
        TEXT("r.Ssao.Intensity"), 1.0f, ECVarFlags::SnapshotPerFrame);

    Core::Console::TConsoleVariable<i32> rVertexLayoutUseShaderReflection(
        TEXT("r.VertexLayout.UseShaderReflection"), 1, ECVarFlags::SnapshotPerFrame);
} // namespace AltinaEngine::Rendering
