#include "Application/Application.h"

#include "Logging/Log.h"
#include "RhiModule.h"

namespace AltinaEngine::Application {
    namespace {
        auto NormalizeWindowProperties(FPlatformWindowProperty Properties)
            -> FPlatformWindowProperty {
            if (Properties.mWidth == 0U) {
                Properties.mWidth = 1U;
            }

            if (Properties.mHeight == 0U) {
                Properties.mHeight = 1U;
            }

            if (Properties.mTitle.IsEmptyString()) {
                Properties.mTitle.Assign(TEXT("AltinaEngine"));
            }

            return Properties;
        }
    } // namespace

    FApplication::FApplication(const FStartupParameters& InStartupParameters)
        : mStartupParameters(InStartupParameters) {}

    FApplication::~FApplication() { Shutdown(); }

    void FApplication::Initialize() {
        Rhi::FRhiModule::LogHelloWorld();

        if (mIsRunning) {
            return;
        }

        EnsureWindow();
        if (!mMainWindow) {
            LogError(TEXT("Failed to create platform window."));
            return;
        }

        mMainWindow->Show();
        mIsRunning = true;

        LogInfo(TEXT("AltinaEngine application initialized."));
    }

    void FApplication::Tick(float InDeltaTime) {
        if (!mIsRunning) {
            return;
        }

        PumpPlatformMessages();

        if (!mIsRunning) {
            return;
        }

        LogInfo(TEXT("AltinaEngine application tick: {}s"), InDeltaTime);
    }

    void FApplication::Shutdown() {
        if (!mIsRunning) {
            return;
        }

        if (mMainWindow) {
            mMainWindow->Hide();
            mMainWindow.Reset();
        }

        mIsRunning = false;
        LogInfo(TEXT("AltinaEngine application shutdown."));
    }

    void FApplication::SetWindowProperties(const FPlatformWindowProperty& InProperties) {
        if (mIsRunning) {
            LogWarning(TEXT("Cannot update window properties while the application is running."));
            return;
        }

        mWindowProperties = NormalizeWindowProperties(InProperties);
    }

    auto FApplication::GetWindowProperties() const noexcept -> const FPlatformWindowProperty& {
        return mWindowProperties;
    }

    auto FApplication::GetStartupParameters() const noexcept -> const FStartupParameters& {
        return mStartupParameters;
    }

    auto FApplication::GetMainWindow() noexcept -> FPlatformWindow* { return mMainWindow.Get(); }

    void FApplication::RequestShutdown() noexcept { mIsRunning = false; }

    void FApplication::PumpPlatformMessages() {}

    void FApplication::EnsureWindow() {
        if (mMainWindow) {
            return;
        }

        const FPlatformWindowProperty normalizedProperties =
            NormalizeWindowProperties(mWindowProperties);

        FWindowOwner platformWindow = CreatePlatformWindow();
        if (!platformWindow) {
            LogError(TEXT("CreatePlatformWindow returned null."));
            return;
        }

        if (!platformWindow->Initialize(normalizedProperties)) {
            LogError(TEXT("Platform window initialization failed."));
            return;
        }

        mWindowProperties = platformWindow->GetProperties();
        mMainWindow       = Move(platformWindow);
    }

} // namespace AltinaEngine::Application
