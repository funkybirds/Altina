#include "SpaceshipNativeContext.h"

namespace AltinaEngine::Demo::SpaceshipGame::NativeScripts {
    namespace {
        AltinaEngine::Input::FInputSystem*          gInput  = nullptr;
        AltinaEngine::Application::FPlatformWindow* gWindow = nullptr;
    } // namespace

    void SetNativeScriptContext(
        Input::FInputSystem* input, Application::FPlatformWindow* window) noexcept {
        gInput  = input;
        gWindow = window;
    }

    auto GetInput() noexcept -> Input::FInputSystem* { return gInput; }

    auto GetWindow() noexcept -> Application::FPlatformWindow* { return gWindow; }
} // namespace AltinaEngine::Demo::SpaceshipGame::NativeScripts
