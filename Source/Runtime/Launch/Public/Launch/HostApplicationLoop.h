#pragma once

#include "Base/LaunchAPI.h"
#include "Launch/RuntimeSession.h"

namespace AltinaEngine::Launch {
    class AE_LAUNCH_API IRuntimeHostHooks {
    public:
        virtual ~IRuntimeHostHooks() = default;

        virtual auto OnPreInit(IRuntimeSession& session) -> bool {
            (void)session;
            return true;
        }
        virtual auto OnInit(IRuntimeSession& session) -> bool {
            (void)session;
            return true;
        }
        virtual auto LoadCatalog(IRuntimeSession& session) -> bool {
            (void)session;
            return true;
        }
        virtual auto ResolveGraph(IRuntimeSession& session) -> bool {
            (void)session;
            return true;
        }
        virtual auto LoadModules(IRuntimeSession& session) -> bool {
            (void)session;
            return true;
        }
        virtual auto OnHostFrame(IRuntimeSession& session, const FFrameContext& frameContext)
            -> bool {
            (void)session;
            (void)frameContext;
            return true;
        }
        virtual auto ShouldTickSimulation(
            IRuntimeSession& session, const FFrameContext& frameContext) -> bool {
            (void)session;
            (void)frameContext;
            return true;
        }
        virtual auto BuildSimulationTick(
            IRuntimeSession& session, const FFrameContext& frameContext) -> FSimulationTick {
            (void)session;
            FSimulationTick tick{};
            tick.DeltaSeconds = frameContext.DeltaSeconds;
            return tick;
        }
        virtual auto BuildRenderTick(IRuntimeSession& session, const FFrameContext& frameContext)
            -> FRenderTick {
            (void)session;
            (void)frameContext;
            return {};
        }
        virtual void OnAfterFrame(IRuntimeSession& session, const FFrameContext& frameContext) {
            (void)session;
            (void)frameContext;
        }
        virtual void               OnShutdown(IRuntimeSession& session) { (void)session; }
        virtual void               ShutdownModules(IRuntimeSession& session) { (void)session; }
        [[nodiscard]] virtual auto ShouldContinue(
            const IRuntimeSession& session, const FFrameContext& frameContext) const -> bool {
            (void)frameContext;
            return session.IsRunning();
        }
    };

    struct FHostApplicationLoopConfig {
        f32  FixedDeltaSeconds = 1.0f / 60.0f;
        bool SleepPerFrame     = true;
        u32  SleepMilliseconds = 1U;
    };

    class AE_LAUNCH_API FHostApplicationLoop final {
    public:
        auto Run(IRuntimeSession& session, IRuntimeHostHooks& hooks,
            const FHostApplicationLoopConfig& config = {}) const -> int;
    };
} // namespace AltinaEngine::Launch
