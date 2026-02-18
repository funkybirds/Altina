#pragma once

#include "Scripting/ScriptRuntime.h"
#include "Host/HostFxrLoader.h"

namespace AltinaEngine::Scripting::CoreCLR::Interop {
    class FManagedBridge {
    public:
        auto Load(Host::load_assembly_and_get_function_pointer_fn loader,
            const FScriptLoadRequest& request, FScriptHandle& outHandle) -> bool;

        auto Invoke(const FScriptHandle& handle, const FScriptInvocation& invocation) const -> bool;
    };
} // namespace AltinaEngine::Scripting::CoreCLR::Interop
