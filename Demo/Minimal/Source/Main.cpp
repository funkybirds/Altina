#include "Base/AltinaBase.h"
#include "Reflection/Reflection.h"

#if AE_PLATFORM_WIN
    #include "Application/Windows/WindowsApplication.h"
#else
    #error "AltinaEngine Minimal demo currently supports only Windows builds."
#endif

#include <chrono>

using namespace AltinaEngine;
using namespace AltinaEngine::Application;
using namespace AltinaEngine::Core;

struct Neko {
    int    mNya  = 114;
    double mMeow = 1.0;
};

int main(int argc, char** argv) {
    Reflection::RegisterType<Neko>();
    Reflection::RegisterPropertyField<&Neko::mMeow>("Meow");
    Reflection::RegisterPropertyField<&Neko::mNya>("Nya");

    auto                nyaMeta = TypeMeta::FMetaPropertyInfo::Create<&Neko::mNya>();
    Reflection::FObject obj = Reflection::ConstructObject(TypeMeta::FMetaTypeInfo::Create<Neko>());
    auto                propObj = Reflection::GetProperty(obj, nyaMeta);
    auto&               nyaRef  = propObj.As<Container::TRef<int>>().Get();
    nyaRef                      = 514;

    auto& p = obj.As<Neko>();
    LogError(TEXT("Neko mNya value after reflection set: {}"), p.mNya);

    LogWarning(TEXT("Address for &(p.Nya) and &nyaRef: {}, {}"), (u64) & (p.mNya), (u64)&nyaRef);

    FStartupParameters StartupParams{};
    if (argc > 1) {
        StartupParams.mCommandLine = argv[1];
    }

    FWindowsApplication Application(StartupParams);
    Application.Initialize();

    for (i32 FrameIndex = 0; FrameIndex < 600; ++FrameIndex) {
        Application.Tick(1.0f / 60.0f);
        AltinaEngine::Core::Platform::Generic::PlatformSleepMilliseconds(16);

        LogError(TEXT("Frame {} processed."), FrameIndex);
    }

    Application.Shutdown();
    return 0;
}
