#include "Scripting/ScriptRuntimeCoreCLR.h"

#include "Host/RuntimeHost.h"
#include "Interop/ManagedBridge.h"

using AltinaEngine::Core::Container::MakeUniqueAs;

namespace AltinaEngine::Scripting::CoreCLR {
    namespace {
        class FCoreCLRScriptRuntime final : public IScriptRuntime {
        public:
            auto Initialize(const FScriptRuntimeConfig& config) -> bool override {
                return mHost.Initialize(config);
            }

            void Shutdown() override { mHost.Shutdown(); }

            auto Load(const FScriptLoadRequest& request, FScriptHandle& outHandle) -> bool override {
                if (!mHost.IsInitialized()) {
                    return false;
                }
                return mBridge.Load(mHost.GetLoadAssemblyAndGetFunctionPointer(), request, outHandle);
            }

            auto Invoke(const FScriptHandle& handle, const FScriptInvocation& invocation)
                -> bool override {
                return mBridge.Invoke(handle, invocation);
            }

            auto Reload() -> bool override { return mHost.Reload(); }

        private:
            Host::FRuntimeHost     mHost;
            Interop::FManagedBridge mBridge;
        };
    } // namespace

    auto CreateCoreCLRRuntime() -> FScriptRuntimeOwner {
        return MakeUniqueAs<IScriptRuntime, FCoreCLRScriptRuntime>();
    }

    auto CreateCoreCLRRuntime(const FScriptRuntimeConfig& config) -> FScriptRuntimeOwner {
        auto runtime = CreateCoreCLRRuntime();
        if (!runtime) {
            return {};
        }
        if (!runtime->Initialize(config)) {
            return {};
        }
        return runtime;
    }
} // namespace AltinaEngine::Scripting::CoreCLR
