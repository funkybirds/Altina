#pragma once

#include "ScriptingCoreCLRAPI.h"
#include "Scripting/ManagedInterop.h"
#include "Scripting/ManagedRuntimeCoreCLR.h"

namespace AltinaEngine::Input {
    class FInputSystem;
}

namespace AltinaEngine::Scripting::CoreCLR {
    class AE_SCRIPTING_CORECLR_API FScriptSystem {
    public:
        FScriptSystem() = default;
        ~FScriptSystem() = default;

        FScriptSystem(const FScriptSystem&) = delete;
        auto operator=(const FScriptSystem&) -> FScriptSystem& = delete;
        FScriptSystem(FScriptSystem&&) = delete;
        auto operator=(FScriptSystem&&) -> FScriptSystem& = delete;

        auto Initialize(const FScriptRuntimeConfig& runtimeConfig,
            const FManagedRuntimeConfig& managedConfig, const Input::FInputSystem* inputSystem)
            -> bool;
        void Shutdown();

        [[nodiscard]] auto IsInitialized() const noexcept -> bool { return mInitialized; }
        [[nodiscard]] auto GetManagedApi() const noexcept -> const FManagedApi*;

    private:
        FManagedRuntime             mRuntime{};
        FNativeApi                  mNativeApi{};
        const Input::FInputSystem*  mInputSystem = nullptr;
        bool                        mInitialized = false;
    };
} // namespace AltinaEngine::Scripting::CoreCLR
