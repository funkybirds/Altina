#include "Rendering/RenderingSettings.h"

namespace AltinaEngine::Rendering {
    Core::Console::TConsoleVariable<i32> gRendererType(TEXT("rc.RendererType"), 1);

    Core::Console::TConsoleVariable<i32> rIblEnable(TEXT("r.Ibl.Enable"), 1);
    Core::Console::TConsoleVariable<f32> rIblDiffuseIntensity(TEXT("r.Ibl.DiffuseIntensity"), 0.8f);
    Core::Console::TConsoleVariable<f32> rIblSpecularIntensity(
        TEXT("r.Ibl.SpecularIntensity"), 1.0f);
    Core::Console::TConsoleVariable<f32> rIblSaturation(TEXT("r.Ibl.Saturation"), 1.15f);

    Core::Console::TConsoleVariable<i32> rSsaoEnable(TEXT("r.Ssao.Enable"), 1);
    Core::Console::TConsoleVariable<i32> rSsaoSampleCount(TEXT("r.SsaoSampleCount"), 12);
    Core::Console::TConsoleVariable<f32> rSsaoRadiusVS(TEXT("r.Ssao.RadiusVS"), 0.55f);
    Core::Console::TConsoleVariable<f32> rSsaoBiasNdc(TEXT("r.Ssao.BiasNdc"), 0.0005f);
    Core::Console::TConsoleVariable<f32> rSsaoPower(TEXT("r.Ssao.Power"), 1.6f);
    Core::Console::TConsoleVariable<f32> rSsaoIntensity(TEXT("r.Ssao.Intensity"), 1.0f);
} // namespace AltinaEngine::Rendering
