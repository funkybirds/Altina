#include "Host/RuntimeHost.h"

#include "Container/String.h"
#include "Container/StringView.h"
#include "Logging/Log.h"

#include <cstring>
#include <string>
#include <type_traits>
#if AE_PLATFORM_WIN
    #include <cwchar>
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

using AltinaEngine::Core::Logging::LogErrorCat;
using AltinaEngine::Core::Logging::LogWarningCat;
using AltinaEngine::Core::Container::FString;
using AltinaEngine::usize;

namespace AltinaEngine::Scripting::CoreCLR::Host {
    namespace {
        constexpr auto kLogCategory = TEXT("Scripting.CoreCLR");

#if AE_PLATFORM_WIN
        #define AE_HOSTFXR_CALLTYPE __stdcall
#else
        #define AE_HOSTFXR_CALLTYPE
#endif

        auto GetMessageLength(const FHostFxrChar* message) -> usize {
            if (message == nullptr) {
                return 0U;
            }
#if AE_PLATFORM_WIN
            return static_cast<usize>(std::wcslen(message));
#else
            return static_cast<usize>(std::strlen(message));
#endif
        }

        auto HostFxrMessageToFString(const FHostFxrChar* message, usize length) -> FString {
            if (message == nullptr || length == 0U) {
                return {};
            }

            FString out;
            if constexpr (std::is_same_v<AltinaEngine::TChar, char>
                && std::is_same_v<FHostFxrChar, wchar_t>) {
#if AE_PLATFORM_WIN
                const int required = WideCharToMultiByte(
                    CP_UTF8, 0, message, static_cast<int>(length), nullptr, 0, nullptr, nullptr);
                if (required > 0) {
                    std::string utf8(static_cast<size_t>(required), '\0');
                    const int converted = WideCharToMultiByte(
                        CP_UTF8, 0, message, static_cast<int>(length), utf8.data(), required,
                        nullptr, nullptr);
                    if (converted > 0) {
                        out.Append(utf8.c_str(), static_cast<usize>(utf8.size()));
                        return out;
                    }
                }
#endif
                out.Reserve(length);
                for (usize i = 0; i < length; ++i) {
                    out.Append('?');
                }
                return out;
            } else if constexpr (std::is_same_v<AltinaEngine::TChar, wchar_t>
                && std::is_same_v<FHostFxrChar, wchar_t>) {
                out.Reserve(length);
                for (usize i = 0; i < length; ++i) {
                    out.Append(static_cast<AltinaEngine::TChar>(message[i]));
                }
                return out;
            } else if constexpr (std::is_same_v<AltinaEngine::TChar, char>
                && std::is_same_v<FHostFxrChar, char>) {
                out.Reserve(length);
                for (usize i = 0; i < length; ++i) {
                    out.Append(static_cast<AltinaEngine::TChar>(message[i]));
                }
                return out;
            } else {
                out.Reserve(length);
                for (usize i = 0; i < length; ++i) {
                    out.Append(static_cast<AltinaEngine::TChar>(
                        static_cast<unsigned char>(message[i])));
                }
                return out;
            }

            return out;
        }

        void AE_HOSTFXR_CALLTYPE HostFxrErrorWriter(const FHostFxrChar* message) {
            const usize length = GetMessageLength(message);
            if (length == 0U) {
                return;
            }
            const FString text = HostFxrMessageToFString(message, length);
            if (!text.IsEmptyString()) {
                LogErrorCat(kLogCategory, TEXT("hostfxr: {}"), text.ToView());
            }
        }

#undef AE_HOSTFXR_CALLTYPE
    }

    auto FRuntimeHost::Initialize(const FScriptRuntimeConfig& config) -> bool {
        mConfig = config;
        mInitialized = false;
        mLoadAssemblyAndGetFunctionPointer = nullptr;
        mPrevErrorWriter = nullptr;

        if (config.mRuntimeConfigPath.IsEmptyString()) {
            LogErrorCat(kLogCategory, TEXT("Runtime config path is empty."));
            return false;
        }

        if (!mHostFxr.Load(config.mRuntimeConfigPath, config.mRuntimeRoot, config.mDotnetRoot)) {
            LogErrorCat(kLogCategory, TEXT("Failed to load hostfxr."));
            return false;
        }

        if (mHostFxr.GetFunctions().SetErrorWriter) {
            mPrevErrorWriter = mHostFxr.GetFunctions().SetErrorWriter(&HostFxrErrorWriter);
        }

        const auto runtimeConfigPath = ToHostFxrString(config.mRuntimeConfigPath.ToView());
        if (runtimeConfigPath.empty()) {
            LogErrorCat(kLogCategory, TEXT("Runtime config path conversion failed."));
            mHostFxr.Unload();
            return false;
        }

        hostfxr_handle hostHandle = nullptr;
        FHostFxrInitializeParameters params{};
        const FHostFxrInitializeParameters* paramsPtr = nullptr;
        if (!mHostFxr.GetDotnetRoot().empty()) {
            params.mSize = sizeof(FHostFxrInitializeParameters);
            params.mHostPath = nullptr;
            params.mDotnetRoot = mHostFxr.GetDotnetRoot().c_str();
            paramsPtr = &params;
        }

        const i32 initResult = mHostFxr.GetFunctions().InitializeForRuntimeConfig(
            runtimeConfigPath.c_str(), paramsPtr, &hostHandle);
        if (initResult != 0 || hostHandle == nullptr) {
            LogErrorCat(
                kLogCategory, TEXT("hostfxr_initialize_for_runtime_config failed ({})."),
                initResult);
            if (hostHandle != nullptr) {
                mHostFxr.GetFunctions().Close(hostHandle);
            }
            mHostFxr.Unload();
            return false;
        }

        void* delegate = nullptr;
        const i32 delegateResult = mHostFxr.GetFunctions().GetRuntimeDelegate(
            hostHandle, EHostFxrDelegateType::LoadAssemblyAndGetFunctionPointer, &delegate);
        if (delegateResult != 0 || delegate == nullptr) {
            LogErrorCat(kLogCategory, TEXT("hostfxr_get_runtime_delegate failed ({})."),
                delegateResult);
            mHostFxr.GetFunctions().Close(hostHandle);
            mHostFxr.Unload();
            return false;
        }

        mLoadAssemblyAndGetFunctionPointer =
            reinterpret_cast<load_assembly_and_get_function_pointer_fn>(delegate);
        mHostFxr.GetFunctions().Close(hostHandle);

        mInitialized = true;
        return true;
    }

    void FRuntimeHost::Shutdown() {
        mLoadAssemblyAndGetFunctionPointer = nullptr;
        mInitialized = false;
        if (mHostFxr.GetFunctions().SetErrorWriter) {
            mHostFxr.GetFunctions().SetErrorWriter(mPrevErrorWriter);
        }
        mPrevErrorWriter = nullptr;
        mHostFxr.Unload();
    }

    auto FRuntimeHost::Reload() -> bool {
        if (mConfig.mRuntimeConfigPath.IsEmptyString()) {
            LogWarningCat(kLogCategory, TEXT("Reload requested without a stored config."));
            return false;
        }

        Shutdown();
        return Initialize(mConfig);
    }
} // namespace AltinaEngine::Scripting::CoreCLR::Host
