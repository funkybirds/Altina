#pragma once

#include "Input/InputAPI.h"
#include "Application/AppMessageHandler.h"

namespace AltinaEngine::Input {
    class FInputSystem;

    class AE_INPUT_API FInputMessageHandler final : public Application::IAppMessageHandler {
    public:
        explicit FInputMessageHandler(FInputSystem& InInputSystem);
        ~FInputMessageHandler() override = default;

        void OnWindowResized(Application::FPlatformWindow*,
            const Application::FWindowExtent& InExtent) override;
        void OnWindowFocusGained(Application::FPlatformWindow*) override;
        void OnWindowFocusLost(Application::FPlatformWindow*) override;

        void OnKeyDown(u32 InKeyCode, bool InRepeat) override;
        void OnKeyUp(u32 InKeyCode) override;
        void OnCharInput(u32 InCharCode) override;

        void OnMouseMove(i32 InPositionX, i32 InPositionY) override;
        void OnMouseButtonDown(u32 InButton) override;
        void OnMouseButtonUp(u32 InButton) override;
        void OnMouseWheel(f32 InDelta) override;

    private:
        FInputSystem* mInputSystem = nullptr;
    };
} // namespace AltinaEngine::Input
