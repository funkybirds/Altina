#include "Host/RuntimeHost.h"

#include "Logging/Log.h"

using AltinaEngine::Core::Logging::LogErrorCat;
using AltinaEngine::Core::Logging::LogWarningCat;

namespace AltinaEngine::Scripting::CoreCLR::Host {
    namespace {
        constexpr auto kLogCategory = TEXT("Scripting.CoreCLR");
    }

    auto FRuntimeHost::Initialize(const FScriptRuntimeConfig& config) -> bool {
        mConfig = config;
        mInitialized = false;
        mLoadAssemblyAndGetFunctionPointer = nullptr;

        if (config.mRuntimeConfigPath.IsEmptyString()) {
            LogErrorCat(kLogCategory, TEXT("Runtime config path is empty."));
            return false;
        }

        if (!mHostFxr.Load(config.mRuntimeConfigPath, config.mRuntimeRoot, config.mDotnetRoot)) {
            LogErrorCat(kLogCategory, TEXT("Failed to load hostfxr."));
            return false;
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
