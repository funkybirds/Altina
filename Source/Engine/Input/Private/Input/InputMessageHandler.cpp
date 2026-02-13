#include "Input/InputMessageHandler.h"

#include "Input/InputSystem.h"
#include "Input/Keys.h"
#include "Application/PlatformWindow.h"
#include "Base/AltinaBase.h"

namespace {
    using AltinaEngine::u32;

    auto TryTranslateKeyCode(u32 KeyCode, AltinaEngine::Input::EKey& OutKey) -> bool {
#if AE_PLATFORM_WIN
        constexpr u32 kVkA = 0x41;
        constexpr u32 kVkZ = 0x5A;
        constexpr u32 kVk0 = 0x30;
        constexpr u32 kVk9 = 0x39;

        if (KeyCode >= kVkA && KeyCode <= kVkZ) {
            const u32 offset = KeyCode - kVkA;
            const u32 base = static_cast<u32>(AltinaEngine::Input::EKey::A);
            OutKey = static_cast<AltinaEngine::Input::EKey>(base + offset);
            return true;
        }

        if (KeyCode >= kVk0 && KeyCode <= kVk9) {
            const u32 offset = KeyCode - kVk0;
            const u32 base = static_cast<u32>(AltinaEngine::Input::EKey::Num0);
            OutKey = static_cast<AltinaEngine::Input::EKey>(base + offset);
            return true;
        }

        switch (KeyCode) {
            case 0x1B: // VK_ESCAPE
                OutKey = AltinaEngine::Input::EKey::Escape;
                return true;
            case 0x20: // VK_SPACE
                OutKey = AltinaEngine::Input::EKey::Space;
                return true;
            case 0x0D: // VK_RETURN
                OutKey = AltinaEngine::Input::EKey::Enter;
                return true;
            case 0x09: // VK_TAB
                OutKey = AltinaEngine::Input::EKey::Tab;
                return true;
            case 0x08: // VK_BACK
                OutKey = AltinaEngine::Input::EKey::Backspace;
                return true;
            case 0x25: // VK_LEFT
                OutKey = AltinaEngine::Input::EKey::Left;
                return true;
            case 0x27: // VK_RIGHT
                OutKey = AltinaEngine::Input::EKey::Right;
                return true;
            case 0x26: // VK_UP
                OutKey = AltinaEngine::Input::EKey::Up;
                return true;
            case 0x28: // VK_DOWN
                OutKey = AltinaEngine::Input::EKey::Down;
                return true;
            case 0xA0: // VK_LSHIFT
            case 0x10: // VK_SHIFT
                OutKey = AltinaEngine::Input::EKey::LeftShift;
                return true;
            case 0xA1: // VK_RSHIFT
                OutKey = AltinaEngine::Input::EKey::RightShift;
                return true;
            case 0xA2: // VK_LCONTROL
            case 0x11: // VK_CONTROL
                OutKey = AltinaEngine::Input::EKey::LeftControl;
                return true;
            case 0xA3: // VK_RCONTROL
                OutKey = AltinaEngine::Input::EKey::RightControl;
                return true;
            case 0xA4: // VK_LMENU
            case 0x12: // VK_MENU
                OutKey = AltinaEngine::Input::EKey::LeftAlt;
                return true;
            case 0xA5: // VK_RMENU
                OutKey = AltinaEngine::Input::EKey::RightAlt;
                return true;
            default:
                break;
        }
#else
        (void)KeyCode;
#endif
        OutKey = AltinaEngine::Input::EKey::Unknown;
        return false;
    }
} // namespace

namespace AltinaEngine::Input {
    FInputMessageHandler::FInputMessageHandler(FInputSystem& InInputSystem)
        : mInputSystem(&InInputSystem) {}

    void FInputMessageHandler::OnWindowResized(
        Application::FPlatformWindow*, const Application::FWindowExtent& InExtent) {
        if (mInputSystem != nullptr) {
            mInputSystem->OnWindowResized(InExtent.mWidth, InExtent.mHeight);
        }
    }

    void FInputMessageHandler::OnWindowFocusGained(Application::FPlatformWindow*) {
        if (mInputSystem != nullptr) {
            mInputSystem->OnWindowFocusGained();
        }
    }

    void FInputMessageHandler::OnWindowFocusLost(Application::FPlatformWindow*) {
        if (mInputSystem != nullptr) {
            mInputSystem->OnWindowFocusLost();
        }
    }

    void FInputMessageHandler::OnKeyDown(u32 InKeyCode, bool InRepeat) {
        if (mInputSystem != nullptr) {
            EKey translatedKey = EKey::Unknown;
            if (TryTranslateKeyCode(InKeyCode, translatedKey)) {
                mInputSystem->OnKeyDown(translatedKey, InRepeat);
            }
        }
    }

    void FInputMessageHandler::OnKeyUp(u32 InKeyCode) {
        if (mInputSystem != nullptr) {
            EKey translatedKey = EKey::Unknown;
            if (TryTranslateKeyCode(InKeyCode, translatedKey)) {
                mInputSystem->OnKeyUp(translatedKey);
            }
        }
    }

    void FInputMessageHandler::OnCharInput(u32 InCharCode) {
        if (mInputSystem != nullptr) {
            mInputSystem->OnCharInput(InCharCode);
        }
    }

    void FInputMessageHandler::OnMouseMove(i32 InPositionX, i32 InPositionY) {
        if (mInputSystem != nullptr) {
            mInputSystem->OnMouseMove(InPositionX, InPositionY);
        }
    }

    void FInputMessageHandler::OnMouseButtonDown(u32 InButton) {
        if (mInputSystem != nullptr) {
            mInputSystem->OnMouseButtonDown(InButton);
        }
    }

    void FInputMessageHandler::OnMouseButtonUp(u32 InButton) {
        if (mInputSystem != nullptr) {
            mInputSystem->OnMouseButtonUp(InButton);
        }
    }

    void FInputMessageHandler::OnMouseWheel(f32 InDelta) {
        if (mInputSystem != nullptr) {
            mInputSystem->OnMouseWheel(InDelta);
        }
    }
} // namespace AltinaEngine::Input
