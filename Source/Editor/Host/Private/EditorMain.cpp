#include "CoreMinimal.h"
#include "EditorCore/EditorContext.h"
#include "EditorCore/EditorProjectService.h"
#include "EditorPlaySession/EditorPlaySession.h"
#include "EditorUI/EditorUiModule.h"
#include "EditorViewport/EditorViewportBootstrap.h"
#include "Launch/DemoRuntime.h"
#include "Launch/HostApplicationLoop.h"
#include "Launch/RuntimeSession.h"
#include "Utility/EngineConfig/EngineConfig.h"
#include "DebugGui/DebugGui.h"

using namespace AltinaEngine;

namespace {
    class FEditorHostHooks final : public Launch::IRuntimeHostHooks {
    public:
        auto OnInit(Launch::IRuntimeSession& session) -> bool override {
            if (!CoreModule.Initialize(EditorContext)) {
                return false;
            }

            ViewportBootstrap.EnsureDefaultWorld(session);
            PlaySession.Start();

            auto services = session.GetServices();
            UiModule.RegisterDefaultPanels(services.DebugGuiSystem);
            return true;
        }

        auto OnHostFrame(Launch::IRuntimeSession& session,
            const Launch::FFrameContext&          frameContext) -> bool override {
            (void)frameContext;
            auto       services = session.GetServices();
            const auto commands = UiModule.ConsumeUiCommands();
            for (auto cmd : commands) {
                switch (cmd) {
                    case Editor::UI::EEditorUiCommand::Play:
                        PlaySession.RequestPlay();
                        break;
                    case Editor::UI::EEditorUiCommand::Pause:
                        PlaySession.RequestPause();
                        break;
                    case Editor::UI::EEditorUiCommand::Step:
                        PlaySession.RequestStep();
                        break;
                    case Editor::UI::EEditorUiCommand::Stop:
                        PlaySession.RequestStop();
                        break;
                    case Editor::UI::EEditorUiCommand::Exit:
                        bExitRequested = true;
                        break;
                    default:
                        break;
                }
            }

            const bool allowHotkeys = (services.DebugGuiSystem == nullptr)
                || !services.DebugGuiSystem->WantsCaptureKeyboard();
            PlaySession.HandleFrameInput(services.InputSystem, allowHotkeys);
            return !bExitRequested
                && PlaySession.GetState() != Launch::EEditorRuntimeState::Stopped;
        }

        auto ShouldTickSimulation(Launch::IRuntimeSession& session,
            const Launch::FFrameContext&                   frameContext) -> bool override {
            (void)session;
            (void)frameContext;
            return PlaySession.ShouldTickSimulation();
        }

        auto BuildSimulationTick(Launch::IRuntimeSession& session,
            const Launch::FFrameContext& frameContext) -> Launch::FSimulationTick override {
            (void)session;
            return PlaySession.BuildSimulationTick(frameContext);
        }

        auto BuildRenderTick(Launch::IRuntimeSession& session,
            const Launch::FFrameContext& frameContext) -> Launch::FRenderTick override {
            (void)session;
            (void)frameContext;

            Launch::FRenderTick tick{};
            tick.bRedirectPrimaryViewToOffscreen = true;
            tick.PrimaryViewImageId              = Editor::UI::kEditorViewportImageId;
            const auto viewportRequest           = UiModule.GetViewportRequest();
            if (viewportRequest.bHasContent && viewportRequest.Width > 0U
                && viewportRequest.Height > 0U) {
                tick.RenderWidth  = viewportRequest.Width;
                tick.RenderHeight = viewportRequest.Height;
            }
            return tick;
        }

        void OnAfterFrame(
            Launch::IRuntimeSession& session, const Launch::FFrameContext& frameContext) override {
            (void)session;
            (void)frameContext;
            PlaySession.OnFrameConsumed();
            EditorContext.FrameIndex = frameContext.FrameIndex;
        }

        void OnShutdown(Launch::IRuntimeSession& session) override {
            (void)session;
            PlaySession.Shutdown();
            CoreModule.Shutdown(EditorContext);
        }

        [[nodiscard]] auto ShouldContinue(const Launch::IRuntimeSession& session,
            const Launch::FFrameContext& frameContext) const -> bool override {
            (void)frameContext;
            return session.IsRunning() && !bExitRequested
                && PlaySession.GetState() != Launch::EEditorRuntimeState::Stopped;
        }

    private:
        Editor::Core::FEditorCoreModule            CoreModule{};
        Editor::Core::FEditorContext               EditorContext{};
        Editor::Viewport::FEditorViewportBootstrap ViewportBootstrap{};
        Editor::PlaySession::FEditorPlaySession    PlaySession{};
        Editor::UI::FEditorUiModule                UiModule{};
        bool                                       bExitRequested = false;
    };
} // namespace

int main(int argc, char** argv) {
    FStartupParameters startupParams{};
    if (argc > 1) {
        startupParams.mCommandLine = argv[1];
    }

    Editor::Core::FEditorProjectService  projectService{};
    Editor::Core::FEditorProjectSettings projectSettings{};
    Core::Container::FString             commandLine;
    for (usize i = 0; i < startupParams.mCommandLine.Length(); ++i) {
        commandLine.Append(static_cast<TChar>(startupParams.mCommandLine[i]));
    }
    projectService.LoadFromCommandLine(commandLine, projectSettings);

    Launch::FDemoProjectDescriptor demoDescriptor{};
    if (projectSettings.bLoaded) {
        demoDescriptor.DemoName          = projectSettings.DemoName;
        demoDescriptor.DemoModulePath    = projectSettings.DemoModulePath;
        demoDescriptor.ConfigOverride    = projectSettings.ConfigOverride;
        demoDescriptor.AssetRootOverride = projectSettings.AssetRootOverride;

        if (!demoDescriptor.ConfigOverride.IsEmptyString()) {
            if (!startupParams.mCommandLine.IsEmptyString()) {
                startupParams.mCommandLine.Append(" ");
            }
            startupParams.mCommandLine.Append(
                demoDescriptor.ConfigOverride.CStr(), demoDescriptor.ConfigOverride.Length());
        }
        if (!demoDescriptor.AssetRootOverride.IsEmptyString()) {
            if (!startupParams.mCommandLine.IsEmptyString()) {
                startupParams.mCommandLine.Append(" ");
            }
            startupParams.mCommandLine.Append("-Config:GameClient/AssetRoot=\"");
            startupParams.mCommandLine.Append(
                demoDescriptor.AssetRootOverride.CStr(), demoDescriptor.AssetRootOverride.Length());
            startupParams.mCommandLine.Append("\"");
        }
    }

    Core::Utility::EngineConfig::InitializeGlobalConfig(startupParams);
    auto runtimeSession = Launch::CreateDefaultRuntimeSession(startupParams);
    if (!runtimeSession) {
        return 1;
    }

    FEditorHostHooks                   editorHooks{};
    Launch::FDemoHostHooks             demoHooks(demoDescriptor, &editorHooks);

    Launch::FHostApplicationLoopConfig config{};
    config.FixedDeltaSeconds = 1.0f / 60.0f;
    config.SleepPerFrame     = true;
    config.SleepMilliseconds = 1U;

    Launch::FHostApplicationLoop hostLoop{};
    return hostLoop.Run(*runtimeSession.Get(), demoHooks, config);
}
