#include "CoreMinimal.h"
#include "EditorCore/EditorContext.h"
#include "EditorCore/EditorProjectService.h"
#include "EditorPlaySession/EditorPlaySession.h"
#include "EditorUI/EditorUiModule.h"
#include "EditorViewport/EditorViewportBootstrap.h"
#include "Reflection/JsonSerializer.h"
#include "Engine/GameScene/ComponentRegistry.h"
#include "Launch/DemoRuntime.h"
#include "Launch/EngineLoop.h"
#include "Launch/HostApplicationLoop.h"
#include "Launch/RuntimeSession.h"
#include "Utility/EngineConfig/EngineConfig.h"
#include "Utility/Json.h"
#include "Utility/String/CodeConvert.h"
#include "DebugGui/DebugGui.h"

using namespace AltinaEngine;

namespace {
    using AltinaEngine::Core::Reflection::FJsonSerializer;

    struct FComponentTypeLabel {
        Core::Container::FString mTypeName;
        Core::Container::FString mTypeNamespace;
    };

    auto ToEditorPropertyKind(Core::Utility::Json::EJsonType type)
        -> Editor::UI::EEditorPropertyValueKind {
        using EJsonType = Core::Utility::Json::EJsonType;
        using EKind     = Editor::UI::EEditorPropertyValueKind;
        switch (type) {
            case EJsonType::Bool:
                return EKind::Boolean;
            case EJsonType::Number:
                return EKind::Scalar;
            case EJsonType::String:
                return EKind::String;
            case EJsonType::Array:
                return EKind::Array;
            case EJsonType::Object:
                return EKind::Object;
            default:
                return EKind::Unknown;
        }
    }

    auto FormatJsonValueCompact(const Core::Utility::Json::FJsonValue& value, i32 depth = 0)
        -> Core::Container::FString {
        using Core::Container::FString;
        using Core::Utility::Json::EJsonType;

        constexpr i32 kMaxDepth      = 2;
        constexpr i32 kMaxArrayItems = 4;
        constexpr i32 kMaxObjectKeys = 4;

        switch (value.Type) {
            case EJsonType::Null:
                return FString(TEXT("null"));
            case EJsonType::Bool:
                return FString(value.Bool ? TEXT("true") : TEXT("false"));
            case EJsonType::Number:
                return FString::ToString(value.Number);
            case EJsonType::String:
                return Core::Utility::String::FromUtf8(value.String);
            case EJsonType::Array:
            {
                FString   out(TEXT("["));
                const i32 count = static_cast<i32>(value.Array.Size());
                const i32 limit = (count < kMaxArrayItems) ? count : kMaxArrayItems;
                if (depth >= kMaxDepth) {
                    out.Append(TEXT("..."));
                } else {
                    for (i32 i = 0; i < limit; ++i) {
                        if (i > 0) {
                            out.Append(TEXT(", "));
                        }
                        out.Append(
                            FormatJsonValueCompact(*value.Array[static_cast<usize>(i)], depth + 1));
                    }
                    if (count > limit) {
                        if (limit > 0) {
                            out.Append(TEXT(", "));
                        }
                        out.Append(TEXT("..."));
                    }
                }
                out.Append(TEXT("]"));
                return out;
            }
            case EJsonType::Object:
            {
                FString   out(TEXT("{"));
                const i32 count = static_cast<i32>(value.Object.Size());
                const i32 limit = (count < kMaxObjectKeys) ? count : kMaxObjectKeys;
                if (depth >= kMaxDepth) {
                    out.Append(TEXT("..."));
                } else {
                    for (i32 i = 0; i < limit; ++i) {
                        if (i > 0) {
                            out.Append(TEXT(", "));
                        }
                        out.Append(Core::Utility::String::FromUtf8(
                            value.Object[static_cast<usize>(i)].Key));
                        out.Append(TEXT(": "));
                        out.Append(FormatJsonValueCompact(
                            *value.Object[static_cast<usize>(i)].Value, depth + 1));
                    }
                    if (count > limit) {
                        if (limit > 0) {
                            out.Append(TEXT(", "));
                        }
                        out.Append(TEXT("..."));
                    }
                }
                out.Append(TEXT("}"));
                return out;
            }
            default:
                return FString(TEXT("(unknown)"));
        }
    }

    auto BuildComponentPropertySnapshots(const GameScene::FWorld& world,
        const GameScene::FComponentRegistry& componentRegistry, GameScene::FComponentId componentId)
        -> Core::Container::TVector<Editor::UI::FEditorPropertySnapshot> {
        using Core::Container::TVector;
        using Core::Utility::Json::EJsonType;
        using Core::Utility::Json::FJsonDocument;
        using Editor::UI::FEditorPropertySnapshot;

        TVector<FEditorPropertySnapshot> properties;
        const auto* componentTypeEntry = componentRegistry.Find(componentId.Type);
        if (componentTypeEntry == nullptr || componentTypeEntry->SerializeJson == nullptr) {
            return properties;
        }

        FJsonSerializer serializer{};
        componentTypeEntry->SerializeJson(
            const_cast<GameScene::FWorld&>(world), componentId, serializer);

        FJsonDocument document{};
        if (!document.Parse(serializer.GetText())) {
            return properties;
        }

        const auto* root = document.GetRoot();
        if (root == nullptr || root->Type != EJsonType::Object) {
            return properties;
        }

        properties.Reserve(root->Object.Size());
        for (const auto& propertyPair : root->Object) {
            if (propertyPair.Value == nullptr) {
                continue;
            }

            FEditorPropertySnapshot propertySnapshot{};
            propertySnapshot.mName         = Core::Utility::String::FromUtf8(propertyPair.Key);
            propertySnapshot.mDisplayValue = FormatJsonValueCompact(*propertyPair.Value);
            propertySnapshot.mValueKind    = ToEditorPropertyKind(propertyPair.Value->Type);
            properties.PushBack(Move(propertySnapshot));
        }

        return properties;
    }

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
            objectSnapshot.bIsPrefabRoot         = world->IsPrefabRoot(objectId);

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
                componentSnapshot.mProperties =
                    BuildComponentPropertySnapshots(*world, componentRegistry, componentId);
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
            PlaySession.Start(session);

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
            uiFrameContext.mPlayState =
                (PlaySession.GetState() == Launch::EEditorRuntimeState::Running)
                ? Editor::UI::EEditorUiPlayState::Running
                : Editor::UI::EEditorUiPlayState::Stopped;
            mUiFrameOutput = UiModule.TickUi(uiFrameContext);

            for (auto cmd : mUiFrameOutput.mCommands) {
                switch (cmd) {
                    case Editor::UI::EEditorUiCommand::Play:
                        (void)PlaySession.RequestPlay(session);
                        break;
                    case Editor::UI::EEditorUiCommand::Pause:
                        PlaySession.RequestPause();
                        break;
                    case Editor::UI::EEditorUiCommand::Step:
                        PlaySession.RequestStep();
                        break;
                    case Editor::UI::EEditorUiCommand::Stop:
                        (void)PlaySession.RequestStop(session);
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
            if (allowHotkeys && platformInput != nullptr) {
                if (platformInput->WasKeyPressed(Input::EKey::F5)) {
                    (void)PlaySession.RequestPlay(session);
                }
                if (platformInput->WasKeyPressed(Input::EKey::F8)) {
                    (void)PlaySession.RequestStop(session);
                }
            }
            PlaySession.UpdateEditorCamera(platformInput, services.MainWindow,
                ConvertViewportRequest(mUiFrameOutput.mViewportRequest), frameContext);
            UpdateRuntimeInputRouting(session, platformInput, services.DebugGuiSystem);
            return !bExitRequested;
        }

        auto ShouldTickHostedClient(Launch::IRuntimeSession& session,
            const Launch::FFrameContext&                     frameContext) -> bool override {
            (void)session;
            (void)frameContext;
            return PlaySession.GetState() == Launch::EEditorRuntimeState::Running;
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

            Launch::FRenderTick tick = PlaySession.BuildRenderTick(
                ConvertViewportRequest(mUiFrameOutput.mViewportRequest));
            tick.bRedirectPrimaryViewToOffscreen = true;
            tick.PrimaryViewImageId              = Editor::UI::kEditorViewportImageId;
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
            PlaySession.Shutdown(session.GetServices().MainWindow);
            CoreModule.Shutdown(EditorContext);
        }

        [[nodiscard]] auto ShouldContinue(const Launch::IRuntimeSession& session,
            const Launch::FFrameContext& frameContext) const -> bool override {
            (void)frameContext;
            return session.IsRunning() && !bExitRequested;
        }

    private:
        [[nodiscard]] static auto ConvertViewportRequest(
            const Editor::UI::FEditorViewportRequest& viewportRequest)
            -> Editor::PlaySession::FEditorViewportInteraction {
            Editor::PlaySession::FEditorViewportInteraction interaction{};
            interaction.mWidth           = viewportRequest.Width;
            interaction.mHeight          = viewportRequest.Height;
            interaction.mContentMinX     = viewportRequest.ContentMinX;
            interaction.mContentMinY     = viewportRequest.ContentMinY;
            interaction.bHovered         = viewportRequest.bHovered;
            interaction.bFocused         = viewportRequest.bFocused;
            interaction.bHasContent      = viewportRequest.bHasContent;
            interaction.bUiBlockingInput = viewportRequest.bUiBlockingInput;
            return interaction;
        }

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
            if (!allowRuntimeInput) {
                (void)debugGuiSystem;
                engineLoop->SetRuntimeInputOverride(nullptr);
                return;
            }
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
