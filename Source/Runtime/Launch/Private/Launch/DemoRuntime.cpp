#include "Launch/DemoRuntime.h"

#include "Logging/Log.h"
#include "Platform/PlatformFileSystem.h"
#include "Utility/EngineConfig/EngineConfig.h"
#include "Utility/Filesystem/Path.h"
#include "Utility/Filesystem/PathUtils.h"

namespace AltinaEngine::Launch {
    namespace {
        constexpr const TChar* kDemoLoaderCategory = TEXT("Launch.DemoLoader");

        auto                   CombineCommandLine(const FStartupParameters& startupParameters,
                              const FDemoProjectDescriptor&                 descriptor) -> FStartupParameters {
            FStartupParameters out = startupParameters;
            if (descriptor.ConfigOverride.IsEmptyString()
                && descriptor.AssetRootOverride.IsEmptyString()) {
                return out;
            }

            if (!out.mCommandLine.IsEmptyString()) {
                out.mCommandLine.Append(" ");
            }

            if (!descriptor.ConfigOverride.IsEmptyString()) {
                out.mCommandLine.Append(
                    descriptor.ConfigOverride.CStr(), descriptor.ConfigOverride.Length());
            }

            if (!descriptor.AssetRootOverride.IsEmptyString()) {
                if (!out.mCommandLine.IsEmptyString()) {
                    out.mCommandLine.Append(" ");
                }
                out.mCommandLine.Append("-Config:GameClient/AssetRoot=\"");
                out.mCommandLine.Append(
                    descriptor.AssetRootOverride.CStr(), descriptor.AssetRootOverride.Length());
                out.mCommandLine.Append("\"");
            }

            return out;
        }

        auto ResolveModulePath(FStringView sourcePath) -> Core::Container::FString {
            using Core::Utility::Filesystem::FPath;
            if (sourcePath.IsEmpty()) {
                return {};
            }

            const FPath path(sourcePath);
            if (path.IsAbsolute()) {
                return Core::Container::FString(sourcePath);
            }

            auto exeDir = Core::Platform::GetExecutableDir();
            if (exeDir.IsEmptyString()) {
                return Core::Container::FString(sourcePath);
            }

            FPath combined(exeDir);
            combined = combined / sourcePath;
            return combined.Normalized().GetString();
        }
    } // namespace

    FDemoModuleLoader::~FDemoModuleLoader() { Unload(); }

    auto FDemoModuleLoader::Load(const FDemoProjectDescriptor& descriptor) -> bool {
        Unload();
        if (descriptor.DemoModulePath.IsEmptyString()) {
            return false;
        }

        mLoadedModulePath = ResolveModulePath(descriptor.DemoModulePath);
        mModuleHandle     = Core::Platform::LoadDynamicLibrary(mLoadedModulePath);
        if (mModuleHandle == nullptr) {
            LogErrorCat(kDemoLoaderCategory, TEXT("Failed to load demo module: {}"),
                mLoadedModulePath.ToView());
            return false;
        }

        const auto createFn = reinterpret_cast<FCreateDemoClientFn>(
            Core::Platform::GetDynamicLibrarySymbol(mModuleHandle, "AE_CreateDemoClient"));
        mDestroyFn = reinterpret_cast<FDestroyDemoClientFn>(
            Core::Platform::GetDynamicLibrarySymbol(mModuleHandle, "AE_DestroyDemoClient"));
        if (createFn == nullptr || mDestroyFn == nullptr) {
            LogErrorCat(kDemoLoaderCategory, TEXT("Demo module missing required symbols: {}"),
                mLoadedModulePath.ToView());
            Unload();
            return false;
        }

        mClient = createFn();
        if (mClient == nullptr) {
            LogErrorCat(kDemoLoaderCategory, TEXT("Demo module failed to create client: {}"),
                mLoadedModulePath.ToView());
            Unload();
            return false;
        }

        LogInfoCat(kDemoLoaderCategory, TEXT("Loaded demo module: {}"), mLoadedModulePath.ToView());
        return true;
    }

    void FDemoModuleLoader::Unload() {
        if (mClient != nullptr && mDestroyFn != nullptr) {
            mDestroyFn(mClient);
        }
        mClient    = nullptr;
        mDestroyFn = nullptr;

        if (mModuleHandle != nullptr) {
            Core::Platform::UnloadDynamicLibrary(mModuleHandle);
            mModuleHandle = nullptr;
        }
        mLoadedModulePath.Clear();
    }

    FDemoHostHooks::FDemoHostHooks(
        const FDemoProjectDescriptor& descriptor, IRuntimeHostHooks* wrappedHooks)
        : mDescriptor(descriptor), mWrappedHooks(wrappedHooks) {}

    FDemoHostHooks::~FDemoHostHooks() { mModuleLoader.Unload(); }

    auto FDemoHostHooks::BuildContext(IRuntimeSession& session, const FFrameContext* frameContext)
        -> FDemoRuntimeContext {
        FDemoRuntimeContext context{ session, session.GetServices(), frameContext };
        return context;
    }

    auto FDemoHostHooks::OnPreInit(IRuntimeSession& session) -> bool {
        if (mWrappedHooks != nullptr && !mWrappedHooks->OnPreInit(session)) {
            return false;
        }

        if (mDescriptor.DemoModulePath.IsEmptyString()) {
            return true;
        }

        if (!mModuleLoader.Load(mDescriptor)) {
            LogWarningCat(kDemoLoaderCategory,
                TEXT("Demo module load failed in PreInit; continue with fallback runtime."));
            return true;
        }

        auto* client = mModuleLoader.GetClient();
        if (client == nullptr) {
            return true;
        }
        auto context = BuildContext(session, nullptr);
        return client->OnPreInit(context);
    }

    auto FDemoHostHooks::OnInit(IRuntimeSession& session) -> bool {
        auto* client = mModuleLoader.GetClient();
        if (client == nullptr) {
            return (mWrappedHooks == nullptr) ? true : mWrappedHooks->OnInit(session);
        }
        auto context = BuildContext(session, nullptr);
        if (!client->OnInit(context)) {
            LogWarningCat(kDemoLoaderCategory,
                TEXT("Demo client OnInit failed; fallback to host default world."));
            mModuleLoader.Unload();
        }

        if (mWrappedHooks != nullptr && !mWrappedHooks->OnInit(session)) {
            return false;
        }
        return true;
    }

    auto FDemoHostHooks::LoadCatalog(IRuntimeSession& session) -> bool {
        return (mWrappedHooks == nullptr) ? true : mWrappedHooks->LoadCatalog(session);
    }

    auto FDemoHostHooks::ResolveGraph(IRuntimeSession& session) -> bool {
        return (mWrappedHooks == nullptr) ? true : mWrappedHooks->ResolveGraph(session);
    }

    auto FDemoHostHooks::LoadModules(IRuntimeSession& session) -> bool {
        return (mWrappedHooks == nullptr) ? true : mWrappedHooks->LoadModules(session);
    }

    auto FDemoHostHooks::OnHostFrame(IRuntimeSession& session, const FFrameContext& frameContext)
        -> bool {
        if (mWrappedHooks != nullptr && !mWrappedHooks->OnHostFrame(session, frameContext)) {
            return false;
        }

        auto* client = mModuleLoader.GetClient();
        if (client == nullptr) {
            return true;
        }
        auto context = BuildContext(session, &frameContext);
        return client->OnTick(context, frameContext.DeltaSeconds);
    }

    auto FDemoHostHooks::ShouldTickSimulation(
        IRuntimeSession& session, const FFrameContext& frameContext) -> bool {
        if (mWrappedHooks == nullptr) {
            return true;
        }
        return mWrappedHooks->ShouldTickSimulation(session, frameContext);
    }

    auto FDemoHostHooks::BuildSimulationTick(
        IRuntimeSession& session, const FFrameContext& frameContext) -> FSimulationTick {
        if (mWrappedHooks == nullptr) {
            FSimulationTick tick{};
            tick.DeltaSeconds = frameContext.DeltaSeconds;
            return tick;
        }
        return mWrappedHooks->BuildSimulationTick(session, frameContext);
    }

    auto FDemoHostHooks::BuildRenderTick(
        IRuntimeSession& session, const FFrameContext& frameContext) -> FRenderTick {
        if (mWrappedHooks == nullptr) {
            (void)session;
            (void)frameContext;
            return {};
        }
        return mWrappedHooks->BuildRenderTick(session, frameContext);
    }

    void FDemoHostHooks::OnAfterFrame(IRuntimeSession& session, const FFrameContext& frameContext) {
        if (mWrappedHooks != nullptr) {
            mWrappedHooks->OnAfterFrame(session, frameContext);
        }
    }

    void FDemoHostHooks::OnShutdown(IRuntimeSession& session) {
        auto* client = mModuleLoader.GetClient();
        if (client != nullptr) {
            auto context = BuildContext(session, nullptr);
            client->OnShutdown(context);
        }

        if (mWrappedHooks != nullptr) {
            mWrappedHooks->OnShutdown(session);
        }
    }

    void FDemoHostHooks::ShutdownModules(IRuntimeSession& session) {
        if (mWrappedHooks != nullptr) {
            mWrappedHooks->ShutdownModules(session);
        }
    }

    auto FDemoHostHooks::ShouldContinue(
        const IRuntimeSession& session, const FFrameContext& frameContext) const -> bool {
        if (mWrappedHooks == nullptr) {
            (void)frameContext;
            return session.IsRunning();
        }
        return mWrappedHooks->ShouldContinue(session, frameContext);
    }

    auto RunDemoHost(const FDemoProjectDescriptor& descriptor,
        const FStartupParameters&                  startupParameters) -> int {
        const auto mergedStartup = CombineCommandLine(startupParameters, descriptor);
        Core::Utility::EngineConfig::InitializeGlobalConfig(mergedStartup);
        auto runtimeSession = CreateDefaultRuntimeSession(mergedStartup);
        if (!runtimeSession) {
            return 1;
        }

        FDemoHostHooks             hooks(descriptor, nullptr);

        FHostApplicationLoopConfig config{};
        config.FixedDeltaSeconds = 1.0f / 60.0f;
        config.SleepPerFrame     = true;
        config.SleepMilliseconds = 16U;

        FHostApplicationLoop hostLoop{};
        return hostLoop.Run(*runtimeSession.Get(), hooks, config);
    }
} // namespace AltinaEngine::Launch
