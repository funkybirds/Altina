#pragma once

#include "ScriptingCoreCLRAPI.h"
#include "Scripting/ManagedInterop.h"
#include "Scripting/ManagedRuntimeCoreCLR.h"

namespace AltinaEngine::Input {
    class FInputSystem;
}

namespace AltinaEngine::Scripting::CoreCLR {
    using FGetWorldTranslationFn = bool (*)(
        u32 worldId, u32 ownerIndex, u32 ownerGeneration, FScriptVector3* outValue);
    using FSetWorldTranslationFn = bool (*)(
        u32 worldId, u32 ownerIndex, u32 ownerGeneration, const FScriptVector3* value);

    using FGetLocalTranslationFn = bool (*)(
        u32 worldId, u32 ownerIndex, u32 ownerGeneration, FScriptVector3* outValue);
    using FSetLocalTranslationFn = bool (*)(
        u32 worldId, u32 ownerIndex, u32 ownerGeneration, const FScriptVector3* value);

    using FGetWorldRotationFn = bool (*)(
        u32 worldId, u32 ownerIndex, u32 ownerGeneration, FScriptQuaternion* outValue);
    using FSetWorldRotationFn = bool (*)(
        u32 worldId, u32 ownerIndex, u32 ownerGeneration, const FScriptQuaternion* value);

    using FGetLocalRotationFn = bool (*)(
        u32 worldId, u32 ownerIndex, u32 ownerGeneration, FScriptQuaternion* outValue);
    using FSetLocalRotationFn = bool (*)(
        u32 worldId, u32 ownerIndex, u32 ownerGeneration, const FScriptQuaternion* value);

    AE_SCRIPTING_CORECLR_API void SetTransformAccess(FGetWorldTranslationFn getWorldTranslationFn,
        FSetWorldTranslationFn setWorldTranslationFn, FGetLocalTranslationFn getLocalTranslationFn,
        FSetLocalTranslationFn setLocalTranslationFn, FGetWorldRotationFn getWorldRotationFn,
        FSetWorldRotationFn setWorldRotationFn, FGetLocalRotationFn getLocalRotationFn,
        FSetLocalRotationFn setLocalRotationFn) noexcept;

    class AE_SCRIPTING_CORECLR_API FScriptSystem {
    public:
        FScriptSystem()  = default;
        ~FScriptSystem() = default;

        FScriptSystem(const FScriptSystem&)                    = delete;
        auto operator=(const FScriptSystem&) -> FScriptSystem& = delete;
        FScriptSystem(FScriptSystem&&)                         = delete;
        auto operator=(FScriptSystem&&) -> FScriptSystem&      = delete;

        auto Initialize(const FScriptRuntimeConfig& runtimeConfig,
            const FManagedRuntimeConfig& managedConfig, const Input::FInputSystem* inputSystem)
            -> bool;
        void               Shutdown();

        [[nodiscard]] auto IsInitialized() const noexcept -> bool { return mInitialized; }
        [[nodiscard]] auto GetManagedApi() const noexcept -> const FManagedApi*;

    private:
        FManagedRuntime            mRuntime{};
        FNativeApi                 mNativeApi{};
        const Input::FInputSystem* mInputSystem = nullptr;
        bool                       mInitialized = false;
    };
} // namespace AltinaEngine::Scripting::CoreCLR
