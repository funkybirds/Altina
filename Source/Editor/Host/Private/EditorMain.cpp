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
#include "Math/Vector.h"

using namespace AltinaEngine;

namespace {
    auto EditorStateText(Launch::EEditorRuntimeState state) -> const TChar* {
        switch (state) {
            case Launch::EEditorRuntimeState::Stopped:
                return TEXT("Stopped");
            case Launch::EEditorRuntimeState::Starting:
                return TEXT("Starting");
            case Launch::EEditorRuntimeState::Running:
                return TEXT("Running");
            case Launch::EEditorRuntimeState::Paused:
                return TEXT("Paused");
            case Launch::EEditorRuntimeState::Stepping:
                return TEXT("Stepping");
            case Launch::EEditorRuntimeState::Stopping:
                return TEXT("Stopping");
            default:
                return TEXT("Unknown");
        }
    }

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
            if (services.DebugGuiSystem != nullptr) {
                services.DebugGuiSystem->RegisterOverlay(
                    TEXT("Editor.RuntimeOverlay"), [this](DebugGui::IDebugGui& gui) {
                        constexpr auto kColor = DebugGui::MakeColor32(255, 255, 255, 255);
                        gui.DrawText(Core::Math::FVector2f(14.0f, 14.0f), kColor,
                            TEXT("Editor Host Active"));
                        gui.DrawText(Core::Math::FVector2f(14.0f, 30.0f), kColor,
                            EditorStateText(PlaySession.GetState()));
                        gui.DrawText(Core::Math::FVector2f(14.0f, 46.0f), kColor,
                            TEXT("F5 Play | F6 Pause | F10 Step | F8 Stop"));
                    });
            }
            return true;
        }

        auto OnHostFrame(Launch::IRuntimeSession& session,
            const Launch::FFrameContext&          frameContext) -> bool override {
            (void)frameContext;
            auto services = session.GetServices();
            PlaySession.HandleFrameInput(services.InputSystem);
            return PlaySession.GetState() != Launch::EEditorRuntimeState::Stopped;
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
            return session.IsRunning()
                && PlaySession.GetState() != Launch::EEditorRuntimeState::Stopped;
        }

    private:
        Editor::Core::FEditorCoreModule            CoreModule{};
        Editor::Core::FEditorContext               EditorContext{};
        Editor::Viewport::FEditorViewportBootstrap ViewportBootstrap{};
        Editor::PlaySession::FEditorPlaySession    PlaySession{};
        Editor::UI::FEditorUiModule                UiModule{};
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
