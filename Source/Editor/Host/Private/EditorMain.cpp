#include "CoreMinimal.h"
#include "EditorCore/EditorContext.h"
#include "EditorCore/EditorProjectService.h"
#include "EditorPlaySession/EditorPlaySession.h"
#include "EditorUI/EditorUiModule.h"
#include "EditorViewport/EditorViewportBootstrap.h"
#include "Engine/GameScene/ComponentRegistry.h"
#include "Launch/DemoRuntime.h"
#include "Launch/EngineLoop.h"
#include "Launch/HostApplicationLoop.h"
#include "Launch/RuntimeSession.h"
#include "Utility/EngineConfig/EngineConfig.h"
#include "Utility/String/CodeConvert.h"
#include "DebugGui/DebugGui.h"

using namespace AltinaEngine;

namespace {
    struct FComponentTypeLabel {
        Core::Container::FString mTypeName;
        Core::Container::FString mTypeNamespace;
    };

    auto ParseComponentTypeLabel(Core::Container::FNativeStringView typeName)
        -> FComponentTypeLabel {
        FComponentTypeLabel label{};
        if (typeName.IsEmpty()) {
            label.mTypeName = Core::Container::FString(TEXT("Component"));
            return label;
        }

        Core::Container::FNativeString native(typeName.Data(), typeName.Length());
        auto                           fullName = Core::Utility::String::FromUtf8(native);
        if (fullName.IsEmptyString()) {
            label.mTypeName = Core::Container::FString(TEXT("Component"));
            return label;
        }

        const auto separator = fullName.ToView().RFind(TEXT("::"));
        if (separator == Core::Container::FString::npos) {
            label.mTypeName = fullName;
            return label;
        }

        label.mTypeNamespace = fullName.Substr(0, separator);
        label.mTypeName      = fullName.Substr(separator + 2);
        return label;
    }

    auto BuildHierarchySnapshot(const GameScene::FWorld* world)
        -> Editor::UI::FEditorWorldHierarchySnapshot {
        Editor::UI::FEditorWorldHierarchySnapshot snapshot{};
        if (world == nullptr) {
            return snapshot;
        }

        snapshot.mWorldId    = world->GetWorldId();
        const auto objectIds = world->GetAllGameObjectIds();
        snapshot.mGameObjects.Reserve(objectIds.Size());

        auto& componentRegistry = GameScene::GetComponentRegistry();
        for (const auto& objectId : objectIds) {
            auto objectView = world->Object(objectId);
            if (!objectView.IsValid()) {
                continue;
            }

            Editor::UI::FEditorGameObjectSnapshot objectSnapshot{};
            objectSnapshot.mId.mWorldId    = objectId.WorldId;
            objectSnapshot.mId.mIndex      = objectId.Index;
            objectSnapshot.mId.mGeneration = objectId.Generation;

            auto parent = objectView.GetParent();
            if (parent.IsValid() && parent.WorldId == 0U) {
                parent.WorldId = objectId.WorldId;
            }
            objectSnapshot.mParentId.mWorldId    = parent.WorldId;
            objectSnapshot.mParentId.mIndex      = parent.Index;
            objectSnapshot.mParentId.mGeneration = parent.Generation;
            objectSnapshot.mName                 = objectView.GetName();

            const auto componentIds = objectView.GetAllComponents();
            objectSnapshot.mComponents.Reserve(componentIds.Size());
            for (const auto& componentId : componentIds) {
                Editor::UI::FEditorComponentSnapshot componentSnapshot{};
                componentSnapshot.mId.mType       = static_cast<u64>(componentId.Type);
                componentSnapshot.mId.mIndex      = componentId.Index;
                componentSnapshot.mId.mGeneration = componentId.Generation;

                const auto* typeEntry = componentRegistry.Find(componentId.Type);
                if (typeEntry != nullptr) {
                    const auto parsed                = ParseComponentTypeLabel(typeEntry->TypeName);
                    componentSnapshot.mName          = parsed.mTypeName;
                    componentSnapshot.mTypeName      = parsed.mTypeName;
                    componentSnapshot.mTypeNamespace = parsed.mTypeNamespace;
                } else {
                    componentSnapshot.mName          = Core::Container::FString(TEXT("Component"));
                    componentSnapshot.mTypeName      = componentSnapshot.mName;
                    componentSnapshot.mTypeNamespace = Core::Container::FString();
                }
                objectSnapshot.mComponents.PushBack(Move(componentSnapshot));
            }

            snapshot.mGameObjects.PushBack(Move(objectSnapshot));
        }

        return snapshot;
    }

    class FEditorRuntimeInputRouter final {
    public:
        void Route(const Input::FInputSystem*         source,
            const Editor::UI::FEditorViewportRequest& viewportRequest, bool allowRuntimeInput) {
            mRoutedInput.ClearFrameState();

            if (source == nullptr) {
                SetRuntimeFocus(false);
                return;
            }

            const u32 targetWidth  = (viewportRequest.bHasContent && viewportRequest.Width > 0U)
                 ? viewportRequest.Width
                 : source->GetWindowWidth();
            const u32 targetHeight = (viewportRequest.bHasContent && viewportRequest.Height > 0U)
                ? viewportRequest.Height
                : source->GetWindowHeight();
            mRoutedInput.OnWindowResized(targetWidth, targetHeight);

            const bool hasViewportRect = viewportRequest.bHasContent && viewportRequest.Width > 0U
                && viewportRequest.Height > 0U;
            const i32 rawMouseX = source->GetMouseX();
            const i32 rawMouseY = source->GetMouseY();

            bool      mouseInsideViewport = false;
            if (hasViewportRect) {
                const i32 viewportMaxX =
                    viewportRequest.ContentMinX + static_cast<i32>(viewportRequest.Width);
                const i32 viewportMaxY =
                    viewportRequest.ContentMinY + static_cast<i32>(viewportRequest.Height);
                mouseInsideViewport = rawMouseX >= viewportRequest.ContentMinX
                    && rawMouseX < viewportMaxX && rawMouseY >= viewportRequest.ContentMinY
                    && rawMouseY < viewportMaxY;
            }

            const bool viewportInteractive =
                hasViewportRect && (viewportRequest.bFocused || mouseInsideViewport);
            const bool allowKeyboard =
                allowRuntimeInput && hasViewportRect && !viewportRequest.bUiBlockingInput;
            const bool allowMouse = allowRuntimeInput && viewportInteractive
                && !viewportRequest.bUiBlockingInput && mouseInsideViewport;
            const bool runtimeFocus =
                allowRuntimeInput && hasViewportRect && !viewportRequest.bUiBlockingInput;
            SetRuntimeFocus(runtimeFocus);

            if (!runtimeFocus) {
                return;
            }

            if (allowKeyboard) {
                SyncKeyboard(*source);
            } else {
                ReleaseAllKeys();
            }

            if (allowMouse) {
                i32 mappedX = rawMouseX;
                i32 mappedY = rawMouseY;
                if (hasViewportRect) {
                    mappedX = MapToRuntimeAxis(
                        rawMouseX, viewportRequest.ContentMinX, viewportRequest.Width, targetWidth);
                    mappedY = MapToRuntimeAxis(rawMouseY, viewportRequest.ContentMinY,
                        viewportRequest.Height, targetHeight);
                }
                mRoutedInput.OnMouseMove(mappedX, mappedY);
                SyncMouse(*source);
                const f32 wheelDelta = source->GetMouseWheelDelta();
                if (wheelDelta != 0.0f) {
                    mRoutedInput.OnMouseWheel(wheelDelta);
                }
            } else {
                ReleaseAllMouseButtons();
            }
        }

        [[nodiscard]] auto GetRoutedInput() noexcept -> Input::FInputSystem* {
            return &mRoutedInput;
        }

    private:
        [[nodiscard]] static auto MapToRuntimeAxis(
            i32 raw, i32 min, u32 sourceExtent, u32 targetExtent) -> i32 {
            if (sourceExtent == 0U || targetExtent == 0U) {
                return 0;
            }

            const i32 local        = raw - min;
            i32       clampedLocal = local;
            if (clampedLocal < 0) {
                clampedLocal = 0;
            }
            const i32 maxLocal = static_cast<i32>(sourceExtent) - 1;
            if (clampedLocal > maxLocal) {
                clampedLocal = maxLocal;
            }

            i64 scaled = static_cast<i64>(clampedLocal) * static_cast<i64>(targetExtent);
            scaled /= static_cast<i64>(sourceExtent);
            if (scaled < 0LL) {
                scaled = 0LL;
            }
            const i64 maxTarget = static_cast<i64>(targetExtent) - 1LL;
            if (scaled > maxTarget) {
                scaled = maxTarget;
            }
            return static_cast<i32>(scaled);
        }

        void SetRuntimeFocus(bool focused) {
            if (mHasRuntimeFocus == focused) {
                return;
            }
            mHasRuntimeFocus = focused;
            if (mHasRuntimeFocus) {
                mRoutedInput.OnWindowFocusGained();
            } else {
                mRoutedInput.OnWindowFocusLost();
            }
        }

        void SyncKeyboard(const Input::FInputSystem& source) {
            for (auto key : source.GetKeysReleasedThisFrame()) {
                mRoutedInput.OnKeyUp(key);
            }
            for (auto key : source.GetKeysPressedThisFrame()) {
                mRoutedInput.OnKeyDown(key, false);
            }
            for (auto key : source.GetPressedKeys()) {
                mRoutedInput.OnKeyDown(key, true);
            }
            for (auto ch : source.GetCharInputs()) {
                mRoutedInput.OnCharInput(ch);
            }
        }

        void SyncMouse(const Input::FInputSystem& source) {
            for (auto button : source.GetMouseButtonsReleasedThisFrame()) {
                mRoutedInput.OnMouseButtonUp(button);
            }
            for (auto button : source.GetMouseButtonsPressedThisFrame()) {
                mRoutedInput.OnMouseButtonDown(button);
            }
            for (auto button : source.GetPressedMouseButtons()) {
                mRoutedInput.OnMouseButtonDown(button);
            }
        }

        void ReleaseAllKeys() {
            Core::Container::TVector<Input::EKey> keysToRelease;
            for (auto key : mRoutedInput.GetPressedKeys()) {
                keysToRelease.PushBack(key);
            }
            for (auto key : keysToRelease) {
                mRoutedInput.OnKeyUp(key);
            }
        }

        void ReleaseAllMouseButtons() {
            Core::Container::TVector<u32> buttonsToRelease;
            for (auto button : mRoutedInput.GetPressedMouseButtons()) {
                buttonsToRelease.PushBack(button);
            }
            for (auto button : buttonsToRelease) {
                mRoutedInput.OnMouseButtonUp(button);
            }
        }

        Input::FInputSystem mRoutedInput{};
        bool                mHasRuntimeFocus = false;
    };

    class FEditorHostHooks final : public Launch::IRuntimeHostHooks {
    public:
        explicit FEditorHostHooks(const Editor::Core::FEditorProjectSettings& inProjectSettings)
            : mProjectSettings(inProjectSettings) {}

        auto OnInit(Launch::IRuntimeSession& session) -> bool override {
            if (!CoreModule.Initialize(EditorContext)) {
                return false;
            }

            ViewportBootstrap.EnsureDefaultWorld(session);
            PlaySession.Start();

            auto                          services = session.GetServices();
            Editor::UI::FEditorUiInitDesc uiInitDesc{};
            uiInitDesc.mDebugGuiSystem    = services.DebugGuiSystem;
            uiInitDesc.mAssetRoot         = mProjectSettings.AssetRootOverride.ToView();
            uiInitDesc.mProjectSourcePath = mProjectSettings.SourcePath.ToView();
            UiModule.Initialize(uiInitDesc);
            return true;
        }

        auto OnHostFrame(Launch::IRuntimeSession& session,
            const Launch::FFrameContext&          frameContext) -> bool override {
            (void)frameContext;
            auto        services          = session.GetServices();
            const auto* platformInput     = ResolvePlatformInput(session, services.InputSystem);
            const auto  hierarchySnapshot = (services.WorldManager != nullptr)
                 ? BuildHierarchySnapshot(services.WorldManager->GetActiveWorld())
                 : Editor::UI::FEditorWorldHierarchySnapshot{};

            Editor::UI::FEditorUiFrameContext uiFrameContext{};
            uiFrameContext.mHierarchySnapshot  = &hierarchySnapshot;
            uiFrameContext.bClearCommandBuffer = true;
            mUiFrameOutput                     = UiModule.TickUi(uiFrameContext);

            for (auto cmd : mUiFrameOutput.mCommands) {
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
            PlaySession.HandleFrameInput(platformInput, allowHotkeys);
            UpdateRuntimeInputRouting(session, platformInput, services.DebugGuiSystem);
            return !bExitRequested
                && PlaySession.GetState() != Launch::EEditorRuntimeState::Stopped;
        }

        auto ShouldTickSimulation(Launch::IRuntimeSession& session,
            const Launch::FFrameContext&                   frameContext) -> bool override {
            (void)frameContext;
            (void)session;
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
            const auto viewportRequest           = mUiFrameOutput.mViewportRequest;
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
            if (auto* engineLoop = dynamic_cast<Launch::FEngineLoop*>(&session)) {
                engineLoop->SetRuntimeInputOverride(nullptr);
            }
            UiModule.Shutdown();
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
        [[nodiscard]] static auto ResolvePlatformInput(Launch::IRuntimeSession& session,
            const Input::FInputSystem* fallback) -> const Input::FInputSystem* {
            if (auto* engineLoop = dynamic_cast<Launch::FEngineLoop*>(&session)) {
                return engineLoop->GetPlatformInputSystem();
            }
            return fallback;
        }

        void UpdateRuntimeInputRouting(Launch::IRuntimeSession& session,
            const Input::FInputSystem* platformInput, DebugGui::IDebugGuiSystem* debugGuiSystem) {
            auto* engineLoop = dynamic_cast<Launch::FEngineLoop*>(&session);
            if (engineLoop == nullptr) {
                return;
            }

            if (platformInput == nullptr) {
                engineLoop->SetRuntimeInputOverride(nullptr);
                return;
            }

            const bool allowRuntimeInput = PlaySession.ShouldTickSimulation();
            (void)debugGuiSystem;
            InputRouter.Route(platformInput, mUiFrameOutput.mViewportRequest, allowRuntimeInput);
            engineLoop->SetRuntimeInputOverride(InputRouter.GetRoutedInput());
        }

        Editor::Core::FEditorCoreModule            CoreModule{};
        Editor::Core::FEditorContext               EditorContext{};
        Editor::Viewport::FEditorViewportBootstrap ViewportBootstrap{};
        Editor::PlaySession::FEditorPlaySession    PlaySession{};
        Editor::UI::FEditorUiModule                UiModule{};
        Editor::UI::FEditorUiFrameOutput           mUiFrameOutput{};
        Editor::Core::FEditorProjectSettings       mProjectSettings{};
        FEditorRuntimeInputRouter                  InputRouter{};
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

    FEditorHostHooks                   editorHooks(projectSettings);
    Launch::FDemoHostHooks             demoHooks(demoDescriptor, &editorHooks);

    Launch::FHostApplicationLoopConfig config{};
    config.FixedDeltaSeconds = 1.0f / 60.0f;
    config.SleepPerFrame     = true;
    config.SleepMilliseconds = 1U;

    Launch::FHostApplicationLoop hostLoop{};
    return hostLoop.Run(*runtimeSession.Get(), demoHooks, config);
}
