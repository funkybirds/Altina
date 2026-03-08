#include "TestHarness.h"

#include "Launch/DemoRuntime.h"
#include "Launch/EditorRuntimeController.h"
#include "Launch/HostApplicationLoop.h"

namespace {
    class FStubSession final : public AltinaEngine::Launch::IRuntimeSession {
    public:
        auto PreInit() -> bool override {
            ++PreInitCount;
            PreInited = true;
            return true;
        }

        auto Init() -> bool override {
            ++InitCount;
            Inited  = PreInited;
            Running = Inited;
            return Inited;
        }

        auto BeginFrame(const AltinaEngine::Launch::FFrameContext& frameContext) -> bool override {
            (void)frameContext;
            ++BeginFrameCount;
            if (!Running) {
                return false;
            }
            FrameOpen = true;
            return true;
        }

        void TickSimulation(const AltinaEngine::Launch::FSimulationTick& tick) override {
            if (FrameOpen) {
                ++TickCount;
                LastTick = tick;
            }
        }

        void RenderFrame(const AltinaEngine::Launch::FRenderTick& tick) override {
            (void)tick;
            if (FrameOpen) {
                ++RenderCount;
            }
        }

        void EndFrame() override {
            if (FrameOpen) {
                ++EndFrameCount;
            }
            FrameOpen = false;
        }

        void Shutdown() override {
            ++ShutdownCount;
            Running = false;
            Inited  = false;
        }

        [[nodiscard]] auto GetServices() noexcept
            -> AltinaEngine::Launch::FRuntimeServices override {
            return {};
        }
        [[nodiscard]] auto GetServices() const noexcept
            -> AltinaEngine::Launch::FRuntimeServicesConst override {
            return {};
        }
        [[nodiscard]] auto IsRunning() const noexcept -> bool override { return Running; }

        int                PreInitCount    = 0;
        int                InitCount       = 0;
        int                BeginFrameCount = 0;
        int                TickCount       = 0;
        int                RenderCount     = 0;
        int                EndFrameCount   = 0;
        int                ShutdownCount   = 0;
        bool               Running         = false;

    private:
        bool                                  PreInited = false;
        bool                                  Inited    = false;
        bool                                  FrameOpen = false;
        AltinaEngine::Launch::FSimulationTick LastTick{};
    };

    class FStubHooks final : public AltinaEngine::Launch::IRuntimeHostHooks {
    public:
        auto ShouldContinue(const AltinaEngine::Launch::IRuntimeSession& session,
            const AltinaEngine::Launch::FFrameContext& frameContext) const -> bool override {
            (void)session;
            return frameContext.FrameIndex < 3ULL;
        }
    };

    class FCountingHooks final : public AltinaEngine::Launch::IRuntimeHostHooks {
    public:
        auto OnPreInit(AltinaEngine::Launch::IRuntimeSession& session) -> bool override {
            (void)session;
            ++PreInitCount;
            return true;
        }

        auto OnInit(AltinaEngine::Launch::IRuntimeSession& session) -> bool override {
            (void)session;
            ++InitCount;
            return true;
        }

        auto OnHostFrame(AltinaEngine::Launch::IRuntimeSession& session,
            const AltinaEngine::Launch::FFrameContext&          frameContext) -> bool override {
            (void)session;
            (void)frameContext;
            ++HostFrameCount;
            return true;
        }

        void OnShutdown(AltinaEngine::Launch::IRuntimeSession& session) override {
            (void)session;
            ++ShutdownCount;
        }

        auto ShouldContinue(const AltinaEngine::Launch::IRuntimeSession& session,
            const AltinaEngine::Launch::FFrameContext& frameContext) const -> bool override {
            (void)session;
            return frameContext.FrameIndex < 2ULL;
        }

        int PreInitCount   = 0;
        int InitCount      = 0;
        int HostFrameCount = 0;
        int ShutdownCount  = 0;
    };
} // namespace

TEST_CASE("HostApplicationLoop drives runtime session lifecycle") {
    FStubSession                                     session{};
    FStubHooks                                       hooks{};
    AltinaEngine::Launch::FHostApplicationLoop       loop{};
    AltinaEngine::Launch::FHostApplicationLoopConfig cfg{};
    cfg.FixedDeltaSeconds = 1.0f / 30.0f;
    cfg.SleepPerFrame     = false;

    const int rc = loop.Run(session, hooks, cfg);
    REQUIRE_EQ(rc, 0);
    REQUIRE_EQ(session.PreInitCount, 1);
    REQUIRE_EQ(session.InitCount, 1);
    REQUIRE_EQ(session.BeginFrameCount, 3);
    REQUIRE_EQ(session.TickCount, 3);
    REQUIRE_EQ(session.RenderCount, 3);
    REQUIRE_EQ(session.EndFrameCount, 3);
    REQUIRE_EQ(session.ShutdownCount, 1);
}

TEST_CASE("EditorRuntimeController step returns to paused after one frame") {
    AltinaEngine::Launch::FEditorRuntimeController controller{};
    controller.RequestPlay();
    controller.OnSessionInitialized();
    REQUIRE(controller.ShouldTickSimulation());

    controller.RequestPause();
    REQUIRE(!controller.ShouldTickSimulation());

    controller.RequestStep();
    REQUIRE(controller.ShouldTickSimulation());
    auto tick = controller.BuildSimulationTick(1.0f / 60.0f);
    REQUIRE(tick.bSingleStep);
    controller.OnFrameConsumed();
    REQUIRE(!controller.ShouldTickSimulation());
}

TEST_CASE("EditorRuntimeController clamps time scale") {
    AltinaEngine::Launch::FEditorRuntimeController controller{};
    controller.SetTimeScale(100.0f);
    REQUIRE_CLOSE(controller.GetTimeScale(), 8.0f, 1e-6f);
    controller.SetTimeScale(0.0f);
    REQUIRE_CLOSE(controller.GetTimeScale(), 0.01f, 1e-6f);
}

TEST_CASE("DemoModuleLoader fails for missing module") {
    AltinaEngine::Launch::FDemoModuleLoader      loader{};
    AltinaEngine::Launch::FDemoProjectDescriptor descriptor{};
    descriptor.DemoName       = TEXT("Missing");
    descriptor.DemoModulePath = TEXT("MissingDemoModule.dll");

    REQUIRE(!loader.Load(descriptor));
    REQUIRE(loader.GetClient() == nullptr);
}

TEST_CASE("DemoHostHooks forwards wrapped hooks on fallback path") {
    FStubSession                                 session{};
    FCountingHooks                               wrapped{};
    AltinaEngine::Launch::FDemoProjectDescriptor descriptor{};
    descriptor.DemoName       = TEXT("Missing");
    descriptor.DemoModulePath = TEXT("MissingDemoModule.dll");

    AltinaEngine::Launch::FDemoHostHooks             hooks(descriptor, &wrapped);
    AltinaEngine::Launch::FHostApplicationLoop       loop{};
    AltinaEngine::Launch::FHostApplicationLoopConfig cfg{};
    cfg.FixedDeltaSeconds = 1.0f / 60.0f;
    cfg.SleepPerFrame     = false;

    const int rc = loop.Run(session, hooks, cfg);
    REQUIRE_EQ(rc, 0);
    REQUIRE_EQ(wrapped.PreInitCount, 1);
    REQUIRE_EQ(wrapped.InitCount, 1);
    REQUIRE_EQ(wrapped.HostFrameCount, 2);
    REQUIRE_EQ(wrapped.ShutdownCount, 1);
}

TEST_CASE("RenderTick defaults to swapchain present path") {
    AltinaEngine::Launch::FRenderTick tick{};
    REQUIRE_EQ(tick.RenderWidth, 0U);
    REQUIRE_EQ(tick.RenderHeight, 0U);
    REQUIRE(!tick.bRedirectPrimaryViewToOffscreen);
    REQUIRE_EQ(tick.PrimaryViewImageId, 0ULL);
}
