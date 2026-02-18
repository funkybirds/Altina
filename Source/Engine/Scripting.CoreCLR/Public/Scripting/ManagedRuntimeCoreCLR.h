#pragma once

#include "ScriptingCoreCLRAPI.h"
#include "Scripting/ManagedInterop.h"
#include "Scripting/ScriptRuntime.h"
#include "Container/SmartPtr.h"

namespace AltinaEngine::Scripting::CoreCLR {
    struct FManagedRuntimeConfig {
        Core::Container::FString mAssemblyPath;
        Core::Container::FString mTypeName;
        Core::Container::FString mMethodName;
        Core::Container::FString mDelegateTypeName;
    };

    class AE_SCRIPTING_CORECLR_API FManagedRuntime {
    public:
        FManagedRuntime();
        ~FManagedRuntime();

        FManagedRuntime(const FManagedRuntime&) = delete;
        auto operator=(const FManagedRuntime&) -> FManagedRuntime& = delete;
        FManagedRuntime(FManagedRuntime&&) = delete;
        auto operator=(FManagedRuntime&&) -> FManagedRuntime& = delete;

        auto Initialize(const FScriptRuntimeConfig& runtimeConfig,
            const FManagedRuntimeConfig& managedConfig, const FNativeApi& nativeApi) -> bool;
        void Shutdown();

        [[nodiscard]] auto IsInitialized() const noexcept -> bool { return mInitialized; }
        [[nodiscard]] auto GetManagedApi() const noexcept -> const FManagedApi*;

    private:
        struct FImpl;
        Core::Container::TOwner<FImpl> mImpl;
        FManagedApi mManagedApi{};
        bool mInitialized = false;
    };
} // namespace AltinaEngine::Scripting::CoreCLR
