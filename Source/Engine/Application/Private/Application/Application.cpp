#include "Application/Application.h"

#include "Logging/Log.h"

namespace AltinaEngine::Application
{
    namespace
    {
        FPlatformWindowProperty NormalizeWindowProperties(FPlatformWindowProperty Properties)
        {
            if (Properties.Width == 0U)
            {
                Properties.Width = 1U;
            }

            if (Properties.Height == 0U)
            {
                Properties.Height = 1U;
            }

            if (Properties.Title.IsEmptyString())
            {
                Properties.Title.Assign(TEXT("AltinaEngine"));
            }

            return Properties;
        }
    } // namespace

    FApplication::FApplication(const FStartupParameters& InStartupParameters)
        : mStartupParameters(InStartupParameters)
    {
    }

    FApplication::~FApplication()
    {
        Shutdown();
    }

    void FApplication::Initialize()
    {
        if (mIsRunning)
        {
            return;
        }

        EnsureWindow();
        if (!mMainWindow)
        {
            LogError(TEXT("Failed to create platform window."));
            return;
        }

        mMainWindow->Show();
        mIsRunning = true;

        LogInfo(TEXT("AltinaEngine application initialized."));
    }

    void FApplication::Tick(float InDeltaTime)
    {
        if (!mIsRunning)
        {
            return;
        }

        PumpPlatformMessages();

        if (!mIsRunning)
        {
            return;
        }

        LogInfo(TEXT("AltinaEngine application tick: {}s"), InDeltaTime);
    }

    void FApplication::Shutdown()
    {
        if (!mIsRunning)
        {
            return;
        }

        if (mMainWindow)
        {
            mMainWindow->Hide();
            mMainWindow.reset();
        }

        mIsRunning = false;
        LogInfo(TEXT("AltinaEngine application shutdown."));
    }

    void FApplication::SetWindowProperties(const FPlatformWindowProperty& InProperties)
    {
        if (mIsRunning)
        {
            LogWarning(TEXT("Cannot update window properties while the application is running."));
            return;
        }

        mWindowProperties = NormalizeWindowProperties(InProperties);
    }

    const FPlatformWindowProperty& FApplication::GetWindowProperties() const noexcept
    {
        return mWindowProperties;
    }

    const FStartupParameters& FApplication::GetStartupParameters() const noexcept
    {
        return mStartupParameters;
    }

    FPlatformWindow* FApplication::GetMainWindow() noexcept
    {
        return mMainWindow.get();
    }

    void FApplication::RequestShutdown() noexcept
    {
        mIsRunning = false;
    }

    void FApplication::PumpPlatformMessages()
    {
    }

    void FApplication::EnsureWindow()
    {
        if (mMainWindow)
        {
            return;
        }

        const FPlatformWindowProperty NormalizedProperties = NormalizeWindowProperties(mWindowProperties);

        FWindowOwner PlatformWindow = CreatePlatformWindow();
        if (!PlatformWindow)
        {
            LogError(TEXT("CreatePlatformWindow returned null."));
            return;
        }

        if (!PlatformWindow->Initialize(NormalizedProperties))
        {
            LogError(TEXT("Platform window initialization failed."));
            return;
        }

        mWindowProperties = PlatformWindow->GetProperties();
        mMainWindow = AltinaEngine::Move(PlatformWindow);
    }

} // namespace AltinaEngine::Application
