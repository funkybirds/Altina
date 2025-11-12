#include "Application/Application.h"

#include <iostream>

namespace AltinaEngine
{

    FApplication::FApplication(const FStartupParameters& InStartupParams) : mStartupParameters(InStartupParams) {}

    void FApplication::Initialize()
    {
        if (mIsRunning)
        {
            return;
        }

        std::cout << "AltinaEngine Application Initialized with command line: " << mStartupParameters.CommandLine
                  << '\n';
        mIsRunning = true;
    }

    void FApplication::Tick(float InDeltaTime)
    {
        if (!mIsRunning)
        {
            return;
        }

        std::cout << "AltinaEngine Application Tick: " << InDeltaTime << "s\n";
    }

    void FApplication::Shutdown()
    {
        if (!mIsRunning)
        {
            return;
        }

        std::cout << "AltinaEngine Application Shutdown" << '\n';
        mIsRunning = false;
    }

    bool FApplication::IsRunning() const noexcept { return mIsRunning; }

} // namespace AltinaEngine
