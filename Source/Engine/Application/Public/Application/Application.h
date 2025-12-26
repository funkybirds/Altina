#pragma once

#include "Base/ApplicationAPI.h"
#include "Application/PlatformWindow.h"
#include "Container/SmartPtr.h"

namespace AltinaEngine::Application
{
    using AltinaEngine::Core::Container::TOwner;
    using FWindowOwner = TOwner<FPlatformWindow>;

    class AE_APPLICATION_API FApplication
    {
    public:
        explicit FApplication(const FStartupParameters& InStartupParameters);
        virtual ~FApplication();

        void                                         Initialize();
        void                                         Tick(float InDeltaTime);
        void                                         Shutdown();

        [[nodiscard]] bool                           IsRunning() const noexcept { return mIsRunning; }

        void                                         SetWindowProperties(const FPlatformWindowProperty& InProperties);
        [[nodiscard]] const FPlatformWindowProperty& GetWindowProperties() const noexcept;

    protected:
        [[nodiscard]] const FStartupParameters& GetStartupParameters() const noexcept;
        [[nodiscard]] FPlatformWindow*          GetMainWindow() noexcept;
        void                                    RequestShutdown() noexcept;

        virtual FWindowOwner                    CreatePlatformWindow() = 0;
        virtual void                            PumpPlatformMessages();

    private:
        void                    EnsureWindow();

        FStartupParameters      mStartupParameters;
        FPlatformWindowProperty mWindowProperties;
        FWindowOwner            mMainWindow;
        bool                    mIsRunning = false;
    };

} // namespace AltinaEngine::Application
