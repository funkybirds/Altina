#pragma once

#include "HostFxrLoader.h"
#include "Scripting/ScriptRuntime.h"

namespace AltinaEngine::Scripting::CoreCLR::Host {
    class FRuntimeHost {
    public:
        auto Initialize(const FScriptRuntimeConfig& config) -> bool;
        void Shutdown();
        auto Reload() -> bool;

        [[nodiscard]] auto IsInitialized() const noexcept -> bool { return mInitialized; }
        [[nodiscard]] auto GetLoadAssemblyAndGetFunctionPointer() const noexcept
            -> load_assembly_and_get_function_pointer_fn {
            return mLoadAssemblyAndGetFunctionPointer;
        }

    private:
        FHostFxrLibrary                         mHostFxr;
        load_assembly_and_get_function_pointer_fn mLoadAssemblyAndGetFunctionPointer = nullptr;
        hostfxr_error_writer_fn                 mPrevErrorWriter = nullptr;
        FScriptRuntimeConfig                    mConfig;
        bool                                    mInitialized = false;
    };
} // namespace AltinaEngine::Scripting::CoreCLR::Host
