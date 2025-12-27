#pragma once

#include "Base/ApplicationAPI.h"
#include "Application/PlatformWindow.h"
#include "Container/SmartPtr.h"

namespace AltinaEngine::Application
{
    using Core::Container::TOwner;
    using FWindowOwner = TOwner<FPlatformWindow>; // NOLINT(*-identifier-naming)

    class AE_APPLICATION_API FApplication
    {
    public:
        explicit FApplication(const FStartupParameters& InStartupParameters);
        virtual ~FApplication();

        void               Initialize();
        void               Tick(float InDeltaTime);
        void               Shutdown();

        [[nodiscard]] auto IsRunning() const noexcept -> bool { return mIsRunning; }

        void               SetWindowProperties(const FPlatformWindowProperty& InProperties);
        [[nodiscard]] auto GetWindowProperties() const noexcept -> const FPlatformWindowProperty&;

    protected:
        [[nodiscard]] auto GetStartupParameters() const noexcept -> const FStartupParameters&;
        [[nodiscard]] auto GetMainWindow() noexcept -> FPlatformWindow*;
        void               RequestShutdown() noexcept;

        virtual auto       CreatePlatformWindow() -> FWindowOwner = 0;
        virtual void       PumpPlatformMessages();

    private:
        void                    EnsureWindow();

        FStartupParameters      mStartupParameters;
        FPlatformWindowProperty mWindowProperties;
        FWindowOwner            mMainWindow;
        bool                    mIsRunning = false;
    };

} // namespace AltinaEngine::Application
