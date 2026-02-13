#pragma once

#include "Base/ApplicationAPI.h"
#include "Types/Aliases.h"
#include "Container/Vector.h"

namespace AltinaEngine::Application {
    namespace Container = Core::Container;
    using Container::TVector;
    class FPlatformWindow;
    struct FWindowExtent;

    class AE_APPLICATION_API IAppMessageHandler {
    public:
        virtual ~IAppMessageHandler() = default;

        virtual void OnWindowCreated([[maybe_unused]] FPlatformWindow* InWindow) {}
        virtual void OnWindowCloseRequested([[maybe_unused]] FPlatformWindow* InWindow) {}
        virtual void OnWindowClosed([[maybe_unused]] FPlatformWindow* InWindow) {}
        virtual void OnWindowResized([[maybe_unused]] FPlatformWindow* InWindow,
            [[maybe_unused]] const FWindowExtent&                      InExtent) {}
        virtual void OnWindowMoved([[maybe_unused]] FPlatformWindow* InWindow,
            [[maybe_unused]] i32 InPositionX, [[maybe_unused]] i32 InPositionY) {}
        virtual void OnWindowFocusGained([[maybe_unused]] FPlatformWindow* InWindow) {}
        virtual void OnWindowFocusLost([[maybe_unused]] FPlatformWindow* InWindow) {}
        virtual void OnWindowMinimized([[maybe_unused]] FPlatformWindow* InWindow) {}
        virtual void OnWindowMaximized([[maybe_unused]] FPlatformWindow* InWindow) {}
        virtual void OnWindowRestored([[maybe_unused]] FPlatformWindow* InWindow) {}
        virtual void OnWindowDpiScaleChanged(
            [[maybe_unused]] FPlatformWindow* InWindow, [[maybe_unused]] f32 InDpiScale) {}

        virtual void OnKeyDown([[maybe_unused]] u32 InKeyCode, [[maybe_unused]] bool InRepeat) {}
        virtual void OnKeyUp([[maybe_unused]] u32 InKeyCode) {}
        virtual void OnCharInput([[maybe_unused]] u32 InCharCode) {}

        virtual void OnMouseMove(
            [[maybe_unused]] i32 InPositionX, [[maybe_unused]] i32 InPositionY) {}
        virtual void OnMouseEnter() {}
        virtual void OnMouseLeave() {}
        virtual void OnMouseButtonDown([[maybe_unused]] u32 InButton) {}
        virtual void OnMouseButtonUp([[maybe_unused]] u32 InButton) {}
        virtual void OnMouseWheel([[maybe_unused]] f32 InDelta) {}
    };

    class AE_APPLICATION_API FAppMessageRouter final {
    public:
        void RegisterHandler(IAppMessageHandler* InHandler);
        void UnregisterHandler(IAppMessageHandler* InHandler);

        void BroadcastWindowCreated(FPlatformWindow* InWindow);
        void BroadcastWindowCloseRequested(FPlatformWindow* InWindow);
        void BroadcastWindowClosed(FPlatformWindow* InWindow);
        void BroadcastWindowResized(FPlatformWindow* InWindow, const FWindowExtent& InExtent);
        void BroadcastWindowMoved(FPlatformWindow* InWindow, i32 InPositionX, i32 InPositionY);
        void BroadcastWindowFocusGained(FPlatformWindow* InWindow);
        void BroadcastWindowFocusLost(FPlatformWindow* InWindow);
        void BroadcastWindowMinimized(FPlatformWindow* InWindow);
        void BroadcastWindowMaximized(FPlatformWindow* InWindow);
        void BroadcastWindowRestored(FPlatformWindow* InWindow);
        void BroadcastWindowDpiScaleChanged(FPlatformWindow* InWindow, f32 InDpiScale);

        void BroadcastKeyDown(u32 InKeyCode, bool InRepeat);
        void BroadcastKeyUp(u32 InKeyCode);
        void BroadcastCharInput(u32 InCharCode);

        void BroadcastMouseMove(i32 InPositionX, i32 InPositionY);
        void BroadcastMouseEnter();
        void BroadcastMouseLeave();
        void BroadcastMouseButtonDown(u32 InButton);
        void BroadcastMouseButtonUp(u32 InButton);
        void BroadcastMouseWheel(f32 InDelta);

    private:
        TVector<IAppMessageHandler*> mHandlers;
    };
} // namespace AltinaEngine::Application
