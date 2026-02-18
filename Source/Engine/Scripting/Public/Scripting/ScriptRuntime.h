#pragma once

#include "Scripting/ScriptingAPI.h"

#include "Container/SmartPtr.h"
#include "Container/String.h"
#include "Scripting/ScriptTypes.h"
#include "Types/Traits.h"

namespace AltinaEngine::Scripting {
    using AltinaEngine::Move;
    namespace Container = ::AltinaEngine::Core::Container;
    using Container::FString;
    using Container::TOwner;
    using Container::TPolymorphicDeleter;

    struct FScriptRuntimeConfig {
        FString mRuntimeConfigPath;
        FString mRuntimeRoot;
        FString mDotnetRoot;
        bool    mEnableDiagnostics = false;
    };

    struct FScriptLoadRequest {
        FString mAssemblyPath;
        FString mTypeName;
        FString mMethodName;
        FString mDelegateTypeName;
    };

    class AE_SCRIPTING_API IScriptRuntime {
    public:
        virtual ~IScriptRuntime() = default;

        virtual auto Initialize(const FScriptRuntimeConfig& config) -> bool = 0;
        virtual void Shutdown()                                 = 0;
        virtual auto Load(const FScriptLoadRequest& request, FScriptHandle& outHandle) -> bool = 0;
        virtual auto Invoke(const FScriptHandle& handle, const FScriptInvocation& invocation)
            -> bool = 0;
        virtual auto Reload() -> bool { return false; }
    };

    using FScriptRuntimeOwner = TOwner<IScriptRuntime, TPolymorphicDeleter<IScriptRuntime>>;

    class AE_SCRIPTING_API FScriptingRuntime {
    public:
        FScriptingRuntime() = default;
        explicit FScriptingRuntime(FScriptRuntimeOwner runtime) : mRuntime(Move(runtime)) {}

        [[nodiscard]] auto IsValid() const noexcept -> bool { return static_cast<bool>(mRuntime); }

        auto Initialize(const FScriptRuntimeConfig& config) -> bool {
            if (!mRuntime) {
                return false;
            }
            return mRuntime->Initialize(config);
        }

        void Shutdown() {
            if (mRuntime) {
                mRuntime->Shutdown();
            }
        }

        auto Load(const FScriptLoadRequest& request, FScriptHandle& outHandle) -> bool {
            if (!mRuntime) {
                return false;
            }
            return mRuntime->Load(request, outHandle);
        }

        auto Invoke(const FScriptHandle& handle, const FScriptInvocation& invocation) -> bool {
            if (!mRuntime) {
                return false;
            }
            return mRuntime->Invoke(handle, invocation);
        }

        auto Reload() -> bool {
            if (!mRuntime) {
                return false;
            }
            return mRuntime->Reload();
        }

        [[nodiscard]] auto Get() const noexcept -> IScriptRuntime* { return mRuntime.Get(); }

    private:
        FScriptRuntimeOwner mRuntime;
    };
} // namespace AltinaEngine::Scripting
