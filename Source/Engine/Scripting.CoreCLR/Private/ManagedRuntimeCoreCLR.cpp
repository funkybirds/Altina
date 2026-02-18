#include "Scripting/ManagedRuntimeCoreCLR.h"

#include "Interop/ManagedBridge.h"
#include "Host/RuntimeHost.h"
#include "Logging/Log.h"

using AltinaEngine::Core::Logging::LogErrorCat;

namespace AltinaEngine::Scripting::CoreCLR {
    namespace {
        constexpr auto kLogCategory = TEXT("Scripting.CoreCLR");

    #if AE_PLATFORM_WIN
        using managed_startup_fn = void* (__cdecl*)(const void* nativeApi, i32 nativeApiSize);
    #else
        using managed_startup_fn = void* (*)(const void* nativeApi, i32 nativeApiSize);
    #endif
    }

    struct FManagedRuntime::FImpl {
        Host::FRuntimeHost   Host;
        Interop::FManagedBridge Bridge;
    };

    FManagedRuntime::FManagedRuntime()
        : mImpl(Core::Container::MakeUnique<FImpl>()) {}

    FManagedRuntime::~FManagedRuntime() = default;

    auto FManagedRuntime::Initialize(const FScriptRuntimeConfig& runtimeConfig,
        const FManagedRuntimeConfig& managedConfig, const FNativeApi& nativeApi) -> bool {
        mManagedApi = {};
        mInitialized = false;

        if (!mImpl) {
            return false;
        }

        if (managedConfig.mAssemblyPath.IsEmptyString() || managedConfig.mTypeName.IsEmptyString()
            || managedConfig.mMethodName.IsEmptyString()) {
            LogErrorCat(kLogCategory, TEXT("Managed runtime config is missing required fields."));
            return false;
        }

        if (!mImpl->Host.Initialize(runtimeConfig)) {
            LogErrorCat(kLogCategory, TEXT("Failed to initialize CoreCLR host."));
            return false;
        }

        FScriptLoadRequest request{};
        request.mAssemblyPath = managedConfig.mAssemblyPath;
        request.mTypeName = managedConfig.mTypeName;
        request.mMethodName = managedConfig.mMethodName;
        request.mDelegateTypeName = managedConfig.mDelegateTypeName;

        FScriptHandle handle{};
        if (!mImpl->Bridge.Load(mImpl->Host.GetLoadAssemblyAndGetFunctionPointer(), request, handle)) {
            mImpl->Host.Shutdown();
            LogErrorCat(kLogCategory, TEXT("Failed to load managed startup entry."));
            return false;
        }

        auto startup = reinterpret_cast<managed_startup_fn>(handle.mPointer);
        if (!startup) {
            mImpl->Host.Shutdown();
            LogErrorCat(kLogCategory, TEXT("Managed startup pointer is null."));
            return false;
        }

        const void* managedApiPtr = startup(&nativeApi, static_cast<i32>(sizeof(FNativeApi)));
        if (managedApiPtr == nullptr) {
            mImpl->Host.Shutdown();
            LogErrorCat(kLogCategory, TEXT("Managed startup returned null API table."));
            return false;
        }

        mManagedApi = *reinterpret_cast<const FManagedApi*>(managedApiPtr);
        mInitialized = true;
        return true;
    }

    void FManagedRuntime::Shutdown() {
        mManagedApi = {};
        mInitialized = false;
        if (mImpl) {
            mImpl->Host.Shutdown();
        }
    }

    auto FManagedRuntime::GetManagedApi() const noexcept -> const FManagedApi* {
        if (!mInitialized) {
            return nullptr;
        }
        return &mManagedApi;
    }
} // namespace AltinaEngine::Scripting::CoreCLR
