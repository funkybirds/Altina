#pragma once

namespace AltinaEngine::Application {
    class FPlatformWindow;
}

namespace AltinaEngine::Input {
    class FInputSystem;
}

namespace AltinaEngine::Demo::SpaceshipGame::NativeScripts {
    void SetNativeScriptContext(
        Input::FInputSystem* input, Application::FPlatformWindow* window) noexcept;

    [[nodiscard]] auto GetInput() noexcept -> Input::FInputSystem*;
    [[nodiscard]] auto GetWindow() noexcept -> Application::FPlatformWindow*;
} // namespace AltinaEngine::Demo::SpaceshipGame::NativeScripts
