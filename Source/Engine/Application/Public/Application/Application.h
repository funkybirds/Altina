#pragma once

#include "Base/ApplicationAPI.h"
#include "Application/AppMessageHandler.h"
#include "Application/PlatformWindow.h"
#include "Container/SmartPtr.h"

namespace AltinaEngine::Application {
    using Core::Container::TOwner;
    using Core::Container::TPolymorphicDeleter;
    using FWindowOwner =
        TOwner<FPlatformWindow, TPolymorphicDeleter<FPlatformWindow>>; // NOLINT(*-identifier-naming)

    class AE_APPLICATION_API FApplication {
    public:
        explicit FApplication(const FStartupParameters& InStartupParameters);
        virtual ~FApplication();

        void               Initialize();
        void               Tick(float InDeltaTime);
        void               Shutdown();

        [[nodiscard]] auto IsRunning() const noexcept -> bool { return mIsRunning; }

        void RegisterMessageHandler(IAppMessageHandler* InHandler);
        void UnregisterMessageHandler(IAppMessageHandler* InHandler);

        void               SetWindowProperties(const FPlatformWindowProperty& InProperties);
        [[nodiscard]] auto GetWindowProperties() const noexcept -> const FPlatformWindowProperty&;
        [[nodiscard]] auto GetMainWindow() noexcept -> FPlatformWindow*;

    protected:
        [[nodiscard]] auto GetStartupParameters() const noexcept -> const FStartupParameters&;
        void               RequestShutdown() noexcept;

        virtual auto       CreatePlatformWindow() -> FWindowOwner = 0;
        virtual void       PumpPlatformMessages();
        [[nodiscard]] auto GetMessageRouter() noexcept -> FAppMessageRouter*;
        [[nodiscard]] auto GetMessageRouter() const noexcept -> const FAppMessageRouter*;

    private:
        void                    EnsureWindow();

        FStartupParameters      mStartupParameters;
        FPlatformWindowProperty mWindowProperties;
        FWindowOwner            mMainWindow;
        FAppMessageRouter       mMessageRouter;
        bool                    mIsRunning = false;
    };

} // namespace AltinaEngine::Application
