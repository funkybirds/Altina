#include "Interop/ManagedBridge.h"

#include "Logging/Log.h"

using AltinaEngine::Core::Logging::LogErrorCat;

namespace AltinaEngine::Scripting::CoreCLR::Interop {
    namespace {
        constexpr auto kLogCategory = TEXT("Scripting.CoreCLR");
    }

    auto FManagedBridge::Load(Host::load_assembly_and_get_function_pointer_fn loader,
        const FScriptLoadRequest& request, FScriptHandle& outHandle) -> bool {
        if (!loader) {
            LogErrorCat(kLogCategory, TEXT("CoreCLR loader delegate is null."));
            return false;
        }

        if (request.mAssemblyPath.IsEmptyString() || request.mTypeName.IsEmptyString()
            || request.mMethodName.IsEmptyString()) {
            LogErrorCat(kLogCategory, TEXT("Load request is missing required fields."));
            return false;
        }

        const auto assemblyPath = Host::ToHostFxrString(request.mAssemblyPath.ToView());
        const auto typeName     = Host::ToHostFxrString(request.mTypeName.ToView());
        const auto methodName   = Host::ToHostFxrString(request.mMethodName.ToView());
        const auto delegateType = Host::ToHostFxrString(request.mDelegateTypeName.ToView());

        const Host::FHostFxrChar* delegateTypePtr = nullptr;
        if (!request.mDelegateTypeName.IsEmptyString()) {
            delegateTypePtr = delegateType.c_str();
        }

        void*     entry  = nullptr;
        const i32 result = loader(assemblyPath.c_str(), typeName.c_str(), methodName.c_str(),
            delegateTypePtr, nullptr, &entry);
        if (result != 0 || entry == nullptr) {
            LogErrorCat(
                kLogCategory, TEXT("load_assembly_and_get_function_pointer failed ({})."), result);
            return false;
        }

        outHandle.mPointer = entry;
        return true;
    }

    auto FManagedBridge::Invoke(
        const FScriptHandle& handle, const FScriptInvocation& invocation) const -> bool {
        if (!handle.IsValid()) {
            LogErrorCat(kLogCategory, TEXT("Script handle is invalid."));
            return false;
        }

        auto entry = reinterpret_cast<Host::component_entry_point_fn>(handle.mPointer);
        if (!entry) {
            LogErrorCat(kLogCategory, TEXT("Script entry pointer is null."));
            return false;
        }

        const i32 result = entry(invocation.mArgs, invocation.mSize);
        if (result != 0) {
            LogErrorCat(kLogCategory, TEXT("Script invocation failed ({})."), result);
            return false;
        }

        return true;
    }
} // namespace AltinaEngine::Scripting::CoreCLR::Interop
