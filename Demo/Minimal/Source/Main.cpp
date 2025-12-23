#include "Application/Application.h"

#include <chrono>
#include <cstdint>
#include <thread>

using namespace AltinaEngine;

int main(int argc, char** argv)
{
    FStartupParameters StartupParams{};
    if (argc > 1)
    {
        StartupParams.CommandLine = argv[1];
    }

    FApplication Application(StartupParams);
    Application.Initialize();

    for (i32 FrameIndex = 0; FrameIndex < 3; ++FrameIndex)
    {
        Application.Tick(1.0f / 60.0f);
        std::this_thread::sleep_for(std::chrono::milliseconds(16));

        LogError(TEXT("Frame {} processed."), FrameIndex);
    }

    Application.Shutdown();
    return 0;
}
