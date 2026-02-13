#include "Application/Application.h"

#include "Logging/Log.h"
#include "RhiModule.h"

namespace AltinaEngine::Application {
    namespace Container = Core::Container;
    namespace {
        template <typename Func>
        void ForEachHandler(const Container::TVector<IAppMessageHandler*>& handlers, Func&& fn) {
            for (auto* handler : handlers) {
                if (handler != nullptr) {
                    fn(*handler);
                }
            }
        }
    } // namespace

    void FAppMessageRouter::RegisterHandler(IAppMessageHandler* InHandler) {
        if (InHandler == nullptr) {
            return;
        }

        for (auto* handler : mHandlers) {
            if (handler == InHandler) {
                return;
            }
        }

        mHandlers.PushBack(InHandler);
    }

    void FAppMessageRouter::UnregisterHandler(IAppMessageHandler* InHandler) {
        if (InHandler == nullptr) {
            return;
        }

        const usize handlerCount = mHandlers.Size();
        for (usize index = 0U; index < handlerCount; ++index) {
            if (mHandlers[index] == InHandler) {
                for (usize shiftIndex = index + 1U; shiftIndex < handlerCount; ++shiftIndex) {
                    mHandlers[shiftIndex - 1U] = mHandlers[shiftIndex];
                }
                mHandlers.PopBack();
                break;
            }
        }
    }

    void FAppMessageRouter::BroadcastWindowCreated(FPlatformWindow* InWindow) {
        ForEachHandler(mHandlers,
            [InWindow](IAppMessageHandler& handler) { handler.OnWindowCreated(InWindow); });
    }

    void FAppMessageRouter::BroadcastWindowCloseRequested(FPlatformWindow* InWindow) {
        ForEachHandler(mHandlers,
            [InWindow](IAppMessageHandler& handler) { handler.OnWindowCloseRequested(InWindow); });
    }

    void FAppMessageRouter::BroadcastWindowClosed(FPlatformWindow* InWindow) {
        ForEachHandler(mHandlers,
            [InWindow](IAppMessageHandler& handler) { handler.OnWindowClosed(InWindow); });
    }

    void FAppMessageRouter::BroadcastWindowResized(
        FPlatformWindow* InWindow, const FWindowExtent& InExtent) {
        ForEachHandler(mHandlers, [InWindow, &InExtent](IAppMessageHandler& handler) {
            handler.OnWindowResized(InWindow, InExtent);
        });
    }

    void FAppMessageRouter::BroadcastWindowMoved(
        FPlatformWindow* InWindow, i32 InPositionX, i32 InPositionY) {
        ForEachHandler(
            mHandlers, [InWindow, InPositionX, InPositionY](IAppMessageHandler& handler) {
                handler.OnWindowMoved(InWindow, InPositionX, InPositionY);
            });
    }

    void FAppMessageRouter::BroadcastWindowFocusGained(FPlatformWindow* InWindow) {
        ForEachHandler(mHandlers,
            [InWindow](IAppMessageHandler& handler) { handler.OnWindowFocusGained(InWindow); });
    }

    void FAppMessageRouter::BroadcastWindowFocusLost(FPlatformWindow* InWindow) {
        ForEachHandler(mHandlers,
            [InWindow](IAppMessageHandler& handler) { handler.OnWindowFocusLost(InWindow); });
    }

    void FAppMessageRouter::BroadcastWindowMinimized(FPlatformWindow* InWindow) {
        ForEachHandler(mHandlers,
            [InWindow](IAppMessageHandler& handler) { handler.OnWindowMinimized(InWindow); });
    }

    void FAppMessageRouter::BroadcastWindowMaximized(FPlatformWindow* InWindow) {
        ForEachHandler(mHandlers,
            [InWindow](IAppMessageHandler& handler) { handler.OnWindowMaximized(InWindow); });
    }

    void FAppMessageRouter::BroadcastWindowRestored(FPlatformWindow* InWindow) {
        ForEachHandler(mHandlers,
            [InWindow](IAppMessageHandler& handler) { handler.OnWindowRestored(InWindow); });
    }

    void FAppMessageRouter::BroadcastWindowDpiScaleChanged(
        FPlatformWindow* InWindow, f32 InDpiScale) {
        ForEachHandler(mHandlers, [InWindow, InDpiScale](IAppMessageHandler& handler) {
            handler.OnWindowDpiScaleChanged(InWindow, InDpiScale);
        });
    }

    void FAppMessageRouter::BroadcastKeyDown(u32 InKeyCode, bool InRepeat) {
        ForEachHandler(mHandlers, [InKeyCode, InRepeat](IAppMessageHandler& handler) {
            handler.OnKeyDown(InKeyCode, InRepeat);
        });
    }

    void FAppMessageRouter::BroadcastKeyUp(u32 InKeyCode) {
        ForEachHandler(
            mHandlers, [InKeyCode](IAppMessageHandler& handler) { handler.OnKeyUp(InKeyCode); });
    }

    void FAppMessageRouter::BroadcastCharInput(u32 InCharCode) {
        ForEachHandler(mHandlers,
            [InCharCode](IAppMessageHandler& handler) { handler.OnCharInput(InCharCode); });
    }

    void FAppMessageRouter::BroadcastMouseMove(i32 InPositionX, i32 InPositionY) {
        ForEachHandler(mHandlers, [InPositionX, InPositionY](IAppMessageHandler& handler) {
            handler.OnMouseMove(InPositionX, InPositionY);
        });
    }

    void FAppMessageRouter::BroadcastMouseEnter() {
        ForEachHandler(mHandlers, [](IAppMessageHandler& handler) { handler.OnMouseEnter(); });
    }

    void FAppMessageRouter::BroadcastMouseLeave() {
        ForEachHandler(mHandlers, [](IAppMessageHandler& handler) { handler.OnMouseLeave(); });
    }

    void FAppMessageRouter::BroadcastMouseButtonDown(u32 InButton) {
        ForEachHandler(mHandlers,
            [InButton](IAppMessageHandler& handler) { handler.OnMouseButtonDown(InButton); });
    }

    void FAppMessageRouter::BroadcastMouseButtonUp(u32 InButton) {
        ForEachHandler(mHandlers,
            [InButton](IAppMessageHandler& handler) { handler.OnMouseButtonUp(InButton); });
    }

    void FAppMessageRouter::BroadcastMouseWheel(f32 InDelta) {
        ForEachHandler(
            mHandlers, [InDelta](IAppMessageHandler& handler) { handler.OnMouseWheel(InDelta); });
    }

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
        (void)InDeltaTime;
        // LogInfo(TEXT("AltinaEngine application tick: {}s"), InDeltaTime);
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

    void FApplication::RegisterMessageHandler(IAppMessageHandler* InHandler) {
        mMessageRouter.RegisterHandler(InHandler);
    }

    void FApplication::UnregisterMessageHandler(IAppMessageHandler* InHandler) {
        mMessageRouter.UnregisterHandler(InHandler);
    }

    auto FApplication::GetMessageRouter() noexcept -> FAppMessageRouter* { return &mMessageRouter; }

    auto FApplication::GetMessageRouter() const noexcept -> const FAppMessageRouter* {
        return &mMessageRouter;
    }

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
        mMessageRouter.BroadcastWindowCreated(mMainWindow.Get());
    }

} // namespace AltinaEngine::Application
