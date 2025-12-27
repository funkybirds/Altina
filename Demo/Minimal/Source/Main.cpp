#include "Base/AltinaBase.h"

#if AE_PLATFORM_WIN
    #include "Application/Windows/WindowsApplication.h"
#else
    #error "AltinaEngine Minimal demo currently supports only Windows builds."
#endif

#include <chrono>
#include <cstdint>

using namespace AltinaEngine;
using namespace AltinaEngine::Application;

int main(int argc, char** argv)
{
    FStartupParameters StartupParams{};
    if (argc > 1)
    {
        StartupParams.mCommandLine = argv[1];
    }

    FWindowsApplication Application(StartupParams);
    Application.Initialize();

    for (i32 FrameIndex = 0; FrameIndex < 600; ++FrameIndex)
    {
        Application.Tick(1.0f / 60.0f);
        AltinaEngine::Core::Platform::Generic::PlatformSleepMilliseconds(16);

        LogError(TEXT("Frame {} processed."), FrameIndex);
    }

    Application.Shutdown();
    return 0;
}
