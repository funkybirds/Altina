#pragma once

#include "Base/LaunchAPI.h"
#include "CoreMinimal.h"
#include "Launch/HostApplicationLoop.h"
#include "Platform/PlatformDynamicLibrary.h"

namespace AltinaEngine::Launch {
    using AltinaEngine::Core::Container::FString;
    using AltinaEngine::Core::Container::FStringView;

    struct AE_LAUNCH_API FDemoProjectDescriptor {
        FString DemoName;
        FString DemoModulePath;
        FString ConfigOverride;
        FString AssetRootOverride;
    };

    struct AE_LAUNCH_API FDemoRuntimeContext {
        IRuntimeSession&     Session;
        FRuntimeServices     Services;
        const FFrameContext* FrameContext = nullptr;
    };

    class AE_LAUNCH_API IDemoClient {
    public:
        virtual ~IDemoClient() = default;
        virtual auto OnPreInit(FDemoRuntimeContext& context) -> bool {
            (void)context;
            return true;
        }
        virtual auto OnInit(FDemoRuntimeContext& context) -> bool {
            (void)context;
            return true;
        }
        virtual auto OnTick(FDemoRuntimeContext& context, f32 deltaSeconds) -> bool {
            (void)context;
            (void)deltaSeconds;
            return true;
        }
        virtual void OnShutdown(FDemoRuntimeContext& context) { (void)context; }
    };

    using FCreateDemoClientFn  = IDemoClient* (*)();
    using FDestroyDemoClientFn = void (*)(IDemoClient*);

    class AE_LAUNCH_API FDemoModuleLoader final {
    public:
        ~FDemoModuleLoader();

        auto               Load(const FDemoProjectDescriptor& descriptor) -> bool;
        void               Unload();
        [[nodiscard]] auto GetClient() const noexcept -> IDemoClient* { return mClient; }
        [[nodiscard]] auto IsLoaded() const noexcept -> bool { return mClient != nullptr; }
        [[nodiscard]] auto GetLoadedModulePath() const -> FString { return mLoadedModulePath; }

    private:
        AltinaEngine::Core::Platform::FDynamicLibraryHandle mModuleHandle = nullptr;
        IDemoClient*                                        mClient       = nullptr;
        FDestroyDemoClientFn                                mDestroyFn    = nullptr;
        FString                                             mLoadedModulePath;
    };

    class AE_LAUNCH_API FDemoHostHooks final : public IRuntimeHostHooks {
    public:
        FDemoHostHooks(
            const FDemoProjectDescriptor& descriptor, IRuntimeHostHooks* wrappedHooks = nullptr);
        ~FDemoHostHooks() override;

        auto OnPreInit(IRuntimeSession& session) -> bool override;
        auto OnInit(IRuntimeSession& session) -> bool override;
        auto LoadCatalog(IRuntimeSession& session) -> bool override;
        auto ResolveGraph(IRuntimeSession& session) -> bool override;
        auto LoadModules(IRuntimeSession& session) -> bool override;
        auto OnHostFrame(IRuntimeSession& session, const FFrameContext& frameContext)
            -> bool override;
        auto ShouldTickHostedClient(IRuntimeSession& session, const FFrameContext& frameContext)
            -> bool override;
        auto ShouldTickSimulation(IRuntimeSession& session, const FFrameContext& frameContext)
            -> bool override;
        auto BuildSimulationTick(IRuntimeSession& session, const FFrameContext& frameContext)
            -> FSimulationTick override;
        auto BuildRenderTick(IRuntimeSession& session, const FFrameContext& frameContext)
            -> FRenderTick override;
        void OnAfterFrame(IRuntimeSession& session, const FFrameContext& frameContext) override;
        void OnShutdown(IRuntimeSession& session) override;
        void ShutdownModules(IRuntimeSession& session) override;
        [[nodiscard]] auto ShouldContinue(const IRuntimeSession& session,
            const FFrameContext& frameContext) const -> bool override;

        [[nodiscard]] auto HasDemoClient() const noexcept -> bool {
            return mModuleLoader.GetClient() != nullptr;
        }
        [[nodiscard]] auto GetDemoClient() const noexcept -> IDemoClient* {
            return mModuleLoader.GetClient();
        }

    private:
        [[nodiscard]] auto BuildContext(IRuntimeSession& session, const FFrameContext* frameContext)
            -> FDemoRuntimeContext;

    private:
        FDemoProjectDescriptor mDescriptor;
        IRuntimeHostHooks*     mWrappedHooks = nullptr;
        FDemoModuleLoader      mModuleLoader{};
    };

    AE_LAUNCH_API auto RunDemoHost(const FDemoProjectDescriptor& descriptor,
        const FStartupParameters&                                startupParameters) -> int;
} // namespace AltinaEngine::Launch
