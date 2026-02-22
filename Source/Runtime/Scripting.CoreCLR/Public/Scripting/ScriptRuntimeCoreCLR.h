#pragma once

#include "ScriptingCoreCLRAPI.h"
#include "Scripting/ScriptRuntime.h"

namespace AltinaEngine::Scripting::CoreCLR {
    AE_SCRIPTING_CORECLR_API auto CreateCoreCLRRuntime() -> FScriptRuntimeOwner;
    AE_SCRIPTING_CORECLR_API auto CreateCoreCLRRuntime(const FScriptRuntimeConfig& config)
        -> FScriptRuntimeOwner;
} // namespace AltinaEngine::Scripting::CoreCLR
