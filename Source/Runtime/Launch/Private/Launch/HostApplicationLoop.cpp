#include "Launch/HostApplicationLoop.h"

namespace AltinaEngine::Launch {
    auto FHostApplicationLoop::Run(IRuntimeSession& session, IRuntimeHostHooks& hooks,
        const FHostApplicationLoopConfig& config) const -> int {
        if (!session.PreInit()) {
            session.Shutdown();
            return 1;
        }
        if (!hooks.OnPreInit(session)) {
            session.Shutdown();
            return 1;
        }
        if (!hooks.LoadCatalog(session)) {
            session.Shutdown();
            return 1;
        }
        if (!hooks.ResolveGraph(session)) {
            session.Shutdown();
            return 1;
        }
        if (!hooks.LoadModules(session)) {
            session.Shutdown();
            return 1;
        }
        if (!session.Init()) {
            session.Shutdown();
            return 1;
        }
        if (!hooks.OnInit(session)) {
            session.Shutdown();
            return 1;
        }

        FFrameContext frameContext{};
        frameContext.DeltaSeconds = config.FixedDeltaSeconds;
        frameContext.FrameIndex   = 0ULL;

        while (hooks.ShouldContinue(session, frameContext)) {
            if (!hooks.OnHostFrame(session, frameContext)) {
                break;
            }

            if (!session.BeginFrame(frameContext)) {
                break;
            }
            if (hooks.ShouldTickSimulation(session, frameContext)) {
                session.TickSimulation(hooks.BuildSimulationTick(session, frameContext));
            }
            session.RenderFrame(hooks.BuildRenderTick(session, frameContext));
            session.EndFrame();
            hooks.OnAfterFrame(session, frameContext);

            if (config.SleepPerFrame && config.SleepMilliseconds > 0U) {
                Core::Platform::Generic::PlatformSleepMilliseconds(config.SleepMilliseconds);
            }
            frameContext.FrameIndex += 1ULL;
        }

        hooks.OnShutdown(session);
        hooks.ShutdownModules(session);
        session.Shutdown();
        return 0;
    }
} // namespace AltinaEngine::Launch
