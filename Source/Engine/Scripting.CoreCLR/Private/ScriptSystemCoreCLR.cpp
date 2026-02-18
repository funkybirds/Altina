#include "Scripting/ScriptSystemCoreCLR.h"

#include "Input/InputSystem.h"
#include "Input/Keys.h"
#include "Logging/Log.h"
#include "Container/String.h"
#include "Container/StringView.h"
#include "Types/Aliases.h"

#include <cstring>
#include <string>
#include <type_traits>

#if AE_PLATFORM_WIN
    #ifdef TEXT
        #undef TEXT
    #endif
    #ifndef WIN32_LEAN_AND_MEAN
        #define WIN32_LEAN_AND_MEAN
    #endif
    #ifndef NOMINMAX
        #define NOMINMAX
    #endif
    #include <windows.h>
    #ifdef TEXT
        #undef TEXT
    #endif
    #if defined(AE_UNICODE) || defined(UNICODE) || defined(_UNICODE)
        #define TEXT(str) L##str
    #else
        #define TEXT(str) str
    #endif
#endif

using AltinaEngine::Core::Logging::FLogger;
using AltinaEngine::Core::Logging::ELogLevel;

namespace AltinaEngine::Scripting::CoreCLR {
    namespace {
        constexpr auto kManagedLogCategory = TEXT("Scripting.Managed");
        const Input::FInputSystem* gInputSystem = nullptr;

        auto ToFStringFromUtf8(const char* message) -> Core::Container::FString {
            using Core::Container::FNativeStringView;
            using Core::Container::FString;
            if (message == nullptr || message[0] == '\0') {
                return {};
            }
            const usize length = std::strlen(message);
#if defined(AE_UNICODE) || defined(UNICODE) || defined(_UNICODE)
            FString out;
    #if AE_PLATFORM_WIN
            int wideCount = MultiByteToWideChar(
                CP_UTF8, 0, message, static_cast<int>(length), nullptr, 0);
            if (wideCount <= 0) {
                return out;
            }
            std::wstring wide(static_cast<size_t>(wideCount), L'\0');
            MultiByteToWideChar(CP_UTF8, 0, message, static_cast<int>(length),
                wide.data(), wideCount);
            out.Append(wide.c_str(), wide.size());
    #else
            for (usize i = 0; i < length; ++i) {
                out.Append(static_cast<AltinaEngine::TChar>(
                    static_cast<unsigned char>(message[i])));
            }
    #endif
            return out;
#else
            FNativeStringView view(message, length);
            return FString(view);
#endif
        }

        void LogManagedInfo(const char* message) {
            const auto text = ToFStringFromUtf8(message);
            if (!text.IsEmptyString()) {
                FLogger::Log(ELogLevel::Info, kManagedLogCategory, text.ToView());
            }
        }

        void LogManagedError(const char* message) {
            const auto text = ToFStringFromUtf8(message);
            if (!text.IsEmptyString()) {
                FLogger::Log(ELogLevel::Error, kManagedLogCategory, text.ToView());
            }
        }

        auto IsKeyDown(u16 key) -> bool {
            if (!gInputSystem) {
                return false;
            }
            return gInputSystem->IsKeyDown(static_cast<Input::EKey>(key));
        }

        auto WasKeyPressed(u16 key) -> bool {
            if (!gInputSystem) {
                return false;
            }
            return gInputSystem->WasKeyPressed(static_cast<Input::EKey>(key));
        }

        auto WasKeyReleased(u16 key) -> bool {
            if (!gInputSystem) {
                return false;
            }
            return gInputSystem->WasKeyReleased(static_cast<Input::EKey>(key));
        }

        auto IsMouseButtonDown(u32 button) -> bool {
            if (!gInputSystem) {
                return false;
            }
            return gInputSystem->IsMouseButtonDown(button);
        }

        auto WasMouseButtonPressed(u32 button) -> bool {
            if (!gInputSystem) {
                return false;
            }
            return gInputSystem->WasMouseButtonPressed(button);
        }

        auto WasMouseButtonReleased(u32 button) -> bool {
            if (!gInputSystem) {
                return false;
            }
            return gInputSystem->WasMouseButtonReleased(button);
        }

        auto GetMouseX() -> i32 { return gInputSystem ? gInputSystem->GetMouseX() : 0; }

        auto GetMouseY() -> i32 { return gInputSystem ? gInputSystem->GetMouseY() : 0; }

        auto GetMouseDeltaX() -> i32 { return gInputSystem ? gInputSystem->GetMouseDeltaX() : 0; }

        auto GetMouseDeltaY() -> i32 { return gInputSystem ? gInputSystem->GetMouseDeltaY() : 0; }

        auto GetMouseWheelDelta() -> f32 {
            return gInputSystem ? gInputSystem->GetMouseWheelDelta() : 0.0f;
        }

        auto GetWindowWidth() -> u32 { return gInputSystem ? gInputSystem->GetWindowWidth() : 0U; }

        auto GetWindowHeight() -> u32 {
            return gInputSystem ? gInputSystem->GetWindowHeight() : 0U;
        }

        auto HasFocus() -> bool { return gInputSystem && gInputSystem->HasFocus(); }

        auto GetCharInputCount() -> u32 {
            if (!gInputSystem) {
                return 0U;
            }
            return static_cast<u32>(gInputSystem->GetCharInputs().Size());
        }

        auto GetCharInputAt(u32 index) -> u32 {
            if (!gInputSystem) {
                return 0U;
            }
            const auto& inputs = gInputSystem->GetCharInputs();
            if (index >= static_cast<u32>(inputs.Size())) {
                return 0U;
            }
            return inputs[index];
        }
    } // namespace

    auto FScriptSystem::Initialize(const FScriptRuntimeConfig& runtimeConfig,
        const FManagedRuntimeConfig& managedConfig, const Input::FInputSystem* inputSystem)
        -> bool {
        if (mInitialized) {
            Shutdown();
        }

        mInputSystem = inputSystem;
        gInputSystem = inputSystem;

        mNativeApi = {};
        mNativeApi.LogInfo = &LogManagedInfo;
        mNativeApi.LogError = &LogManagedError;
        mNativeApi.IsKeyDown = &IsKeyDown;
        mNativeApi.WasKeyPressed = &WasKeyPressed;
        mNativeApi.WasKeyReleased = &WasKeyReleased;
        mNativeApi.IsMouseButtonDown = &IsMouseButtonDown;
        mNativeApi.WasMouseButtonPressed = &WasMouseButtonPressed;
        mNativeApi.WasMouseButtonReleased = &WasMouseButtonReleased;
        mNativeApi.GetMouseX = &GetMouseX;
        mNativeApi.GetMouseY = &GetMouseY;
        mNativeApi.GetMouseDeltaX = &GetMouseDeltaX;
        mNativeApi.GetMouseDeltaY = &GetMouseDeltaY;
        mNativeApi.GetMouseWheelDelta = &GetMouseWheelDelta;
        mNativeApi.GetWindowWidth = &GetWindowWidth;
        mNativeApi.GetWindowHeight = &GetWindowHeight;
        mNativeApi.HasFocus = &HasFocus;
        mNativeApi.GetCharInputCount = &GetCharInputCount;
        mNativeApi.GetCharInputAt = &GetCharInputAt;

        if (!mRuntime.Initialize(runtimeConfig, managedConfig, mNativeApi)) {
            gInputSystem = nullptr;
            mInputSystem = nullptr;
            return false;
        }

        const auto* api = mRuntime.GetManagedApi();
        if (!api) {
            mRuntime.Shutdown();
            gInputSystem = nullptr;
            mInputSystem = nullptr;
            return false;
        }

        SetManagedApi(api);
        mInitialized = true;
        return true;
    }

    void FScriptSystem::Shutdown() {
        if (!mInitialized) {
            return;
        }

        ClearManagedApi();
        mRuntime.Shutdown();
        gInputSystem = nullptr;
        mInputSystem = nullptr;
        mInitialized = false;
    }

    auto FScriptSystem::GetManagedApi() const noexcept -> const FManagedApi* {
        return mRuntime.GetManagedApi();
    }
} // namespace AltinaEngine::Scripting::CoreCLR
