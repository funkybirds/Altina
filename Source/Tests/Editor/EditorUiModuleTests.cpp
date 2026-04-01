#include "TestHarness.h"

#include "EditorUI/EditorUiModule.h"
#include "EditorUI/EditorUiTesting.h"
#include "DebugGui/DebugGui.h"
#include "Input/InputSystem.h"

#include <filesystem>
#include <fstream>

namespace {
    auto ToFString(const std::filesystem::path& path) -> AltinaEngine::Core::Container::FString {
        AltinaEngine::Core::Container::FString out;
#if defined(AE_UNICODE) || defined(UNICODE) || defined(_UNICODE)
        const auto wide = path.wstring();
        if (!wide.empty()) {
            out.Append(wide.c_str(), wide.size());
        }
#else
        const auto narrow = path.string();
        if (!narrow.empty()) {
            out.Append(narrow.c_str(), narrow.size());
        }
#endif
        return out;
    }

    void PrepareInput(AltinaEngine::Input::FInputSystem& input, AltinaEngine::u32 w,
        AltinaEngine::u32 h, AltinaEngine::i32 mx, AltinaEngine::i32 my) {
        input.ClearFrameState();
        input.OnWindowResized(w, h);
        input.OnWindowFocusGained();
        input.OnMouseMove(mx, my);
    }

    void InitializeUi(AltinaEngine::Editor::UI::FEditorUiModule& uiModule,
        AltinaEngine::DebugGui::IDebugGuiSystem*                 debugGuiSystem,
        AltinaEngine::Core::Container::FStringView               assetRoot = {}) {
        AltinaEngine::Editor::UI::FEditorUiInitDesc initDesc{};
        initDesc.mDebugGuiSystem = debugGuiSystem;
        initDesc.mAssetRoot      = assetRoot;
        uiModule.Initialize(initDesc);
    }

    auto TickUi(AltinaEngine::Editor::UI::FEditorUiModule&             uiModule,
        const AltinaEngine::Editor::UI::FEditorWorldHierarchySnapshot* snapshot = nullptr,
        bool clearCommands = false) -> AltinaEngine::Editor::UI::FEditorUiFrameOutput {
        AltinaEngine::Editor::UI::FEditorUiFrameContext context{};
        context.mHierarchySnapshot  = snapshot;
        context.bClearCommandBuffer = clearCommands;
        return uiModule.TickUi(context);
    }

    struct FInspectorTestLayout {
        AltinaEngine::i32 mPanelMinX             = 0;
        AltinaEngine::i32 mPanelMinY             = 0;
        AltinaEngine::i32 mPanelMaxX             = 0;
        AltinaEngine::i32 mPanelMaxY             = 0;
        AltinaEngine::i32 mFirstComponentHeaderX = 0;
        AltinaEngine::i32 mFirstComponentHeaderY = 0;
        AltinaEngine::i32 mScrollBarX            = 0;
        AltinaEngine::i32 mScrollBarY            = 0;
        AltinaEngine::i32 mScrollTrackMinY       = 0;
        AltinaEngine::i32 mScrollTrackMaxY       = 0;
    };

    auto BuildInspectorTestLayout(AltinaEngine::DebugGui::IDebugGuiSystem* sys,
        AltinaEngine::u32 displayWidth, AltinaEngine::u32 displayHeight) -> FInspectorTestLayout {
        const auto theme      = sys->GetTheme();
        const auto uiScale    = (theme.mUiScale > 0.01f) ? theme.mUiScale : 1.0f;
        const auto ScalePx    = [uiScale](float value) { return value * uiScale; };
        const auto ClampValue = [](float value, float minValue, float maxValue) {
            if (value < minValue) {
                return minValue;
            }
            if (value > maxValue) {
                return maxValue;
            }
            return value;
        };
        const auto menuBarHeight =
            (theme.mEditor.mMenu.mHeight > 0.0f) ? theme.mEditor.mMenu.mHeight : ScalePx(84.0f);
        const auto workspacePad = (theme.mEditor.mWorkspacePadding > 0.0f)
            ? theme.mEditor.mWorkspacePadding
            : ScalePx(18.0f);
        const auto splitterSize =
            (theme.mEditor.mSplitterSize > 0.0f) ? theme.mEditor.mSplitterSize : ScalePx(10.0f);
        const auto tabBarHeight =
            (theme.mEditor.mTabs.mHeight > 0.0f) ? theme.mEditor.mTabs.mHeight : ScalePx(40.0f);
        const auto panelPadding =
            (theme.mEditor.mPanelPadding > 0.0f) ? theme.mEditor.mPanelPadding : ScalePx(10.0f);
        const auto minPanelWidth =
            (theme.mEditor.mMinPanelWidth > 0.0f) ? theme.mEditor.mMinPanelWidth : ScalePx(140.0f);
        const auto minCenterWidth = (theme.mEditor.mMinCenterWidth > 0.0f)
            ? theme.mEditor.mMinCenterWidth
            : ScalePx(260.0f);
        const auto minTopHeight =
            (theme.mEditor.mMinTopHeight > 0.0f) ? theme.mEditor.mMinTopHeight : ScalePx(180.0f);
        const auto  minBottomHeight = (theme.mEditor.mMinBottomHeight > 0.0f)
             ? theme.mEditor.mMinBottomHeight
             : ScalePx(100.0f);

        const float workspaceMinX = workspacePad;
        const float workspaceMinY = workspacePad + menuBarHeight + workspacePad;
        const float workspaceMaxX = static_cast<float>(displayWidth) - workspacePad;
        const float workspaceMaxY = static_cast<float>(displayHeight) - workspacePad;
        const float workspaceW    = workspaceMaxX - workspaceMinX;
        const float workspaceH    = workspaceMaxY - workspaceMinY;

        float       leftW  = ClampValue(workspaceW * 0.2f, minPanelWidth,
                   workspaceW - minCenterWidth - minPanelWidth - 2.0f * splitterSize);
        float       rightW = ClampValue(workspaceW * 0.28f, minPanelWidth,
                  workspaceW - minCenterWidth - leftW - 2.0f * splitterSize);
        if (leftW + rightW + minCenterWidth + 2.0f * splitterSize > workspaceW) {
            rightW = workspaceW - leftW - minCenterWidth - 2.0f * splitterSize;
            if (rightW < minPanelWidth) {
                rightW = minPanelWidth;
                leftW  = workspaceW - rightW - minCenterWidth - 2.0f * splitterSize;
            }
        }
        const float topH = ClampValue(
            workspaceH * (1.0f - 0.28f), minTopHeight, workspaceH - minBottomHeight - splitterSize);

        const float centerMinX = workspaceMinX + leftW + splitterSize;
        const float centerMaxX = centerMinX + (workspaceW - leftW - rightW - 2.0f * splitterSize);
        const float rightMinX  = centerMaxX + splitterSize;
        const float rightMaxX  = workspaceMaxX;
        const float rightMinY  = workspaceMinY;
        const float rightMaxY  = workspaceMinY + topH;

        const float contentMinX = rightMinX + panelPadding;
        const float contentMinY = rightMinY + tabBarHeight + panelPadding;
        const float contentMaxX = rightMaxX - panelPadding;
        const float contentMaxY = rightMaxY - panelPadding;

        const float panelMinY       = contentMinY;
        const float panelMaxY       = contentMaxY;
        const float outerPad        = ScalePx(4.0f);
        const float contentClipMinY = panelMinY + outerPad;
        const float scrollBarWidth =
            ((theme.mScrollBarWidth > 0.0f) ? theme.mScrollBarWidth : 8.0f) * uiScale;
        const float          scrollTrackMinX = contentMaxX - outerPad - scrollBarWidth;
        const float          scrollTrackMaxX = contentMaxX - outerPad;
        const float          scrollTrackMinY = panelMinY + outerPad;
        const float          scrollTrackMaxY = panelMaxY - outerPad;
        const float          scrollTrackH    = scrollTrackMaxY - scrollTrackMinY;

        FInspectorTestLayout layout{};
        layout.mPanelMinX                  = static_cast<AltinaEngine::i32>(contentMinX);
        layout.mPanelMinY                  = static_cast<AltinaEngine::i32>(contentMinY);
        layout.mPanelMaxX                  = static_cast<AltinaEngine::i32>(contentMaxX);
        layout.mPanelMaxY                  = static_cast<AltinaEngine::i32>(contentMaxY);
        const float rowHeight              = ScalePx(20.0f);
        const float rowGap                 = ScalePx(2.0f);
        const float buttonHeight           = ScalePx(22.0f);
        const float sectionGap             = ScalePx(8.0f);
        const float headerGap              = ScalePx(4.0f);
        const float basicHeaderHeight      = ScalePx(16.0f);
        const float componentsHeaderHeight = ScalePx(16.0f);
        const float collapseHeaderHeight   = ScalePx(26.0f);
        const float firstComponentHeaderY  = contentClipMinY + basicHeaderHeight + headerGap
            + (rowHeight + rowGap) * 2.0f + (buttonHeight + rowGap) * 2.0f + buttonHeight
            + sectionGap + componentsHeaderHeight + headerGap;
        layout.mFirstComponentHeaderX =
            static_cast<AltinaEngine::i32>(contentMinX + ScalePx(48.0f));
        layout.mFirstComponentHeaderY =
            static_cast<AltinaEngine::i32>(firstComponentHeaderY + collapseHeaderHeight * 0.5f);
        layout.mScrollBarX = static_cast<AltinaEngine::i32>(
            scrollTrackMinX + (scrollTrackMaxX - scrollTrackMinX) * 0.5f);
        layout.mScrollBarY = static_cast<AltinaEngine::i32>(scrollTrackMinY + scrollTrackH * 0.75f);
        layout.mScrollTrackMinY = static_cast<AltinaEngine::i32>(scrollTrackMinY);
        layout.mScrollTrackMaxY = static_cast<AltinaEngine::i32>(scrollTrackMaxY);
        return layout;
    }
} // namespace

TEST_CASE("EditorUiModule reports viewport render request after registration") {
    using AltinaEngine::DebugGui::CreateDebugGuiSystem;
    using AltinaEngine::DebugGui::DestroyDebugGuiSystem;

    auto* sys = CreateDebugGuiSystem();
    REQUIRE(sys != nullptr);
    sys->SetEnabled(true);
    sys->SetShowStats(false);
    sys->SetShowConsole(false);
    sys->SetShowCVars(false);

    AltinaEngine::Editor::UI::FEditorUiModule uiModule{};
    InitializeUi(uiModule, sys);

    AltinaEngine::Input::FInputSystem input;
    PrepareInput(input, 1280, 720, 300, 200);
    sys->TickGameThread(input, 1.0f / 60.0f, 1280, 720);

    const auto output  = TickUi(uiModule);
    const auto request = output.mViewportRequest;
    REQUIRE(request.bHasContent);
    REQUIRE(request.Width > 0U);
    REQUIRE(request.Height > 0U);

    uiModule.Shutdown();
    DestroyDebugGuiSystem(sys);
}

TEST_CASE("EditorUiModule asset panel scan ignores meta files") {
    using AltinaEngine::Core::Container::FString;
    using AltinaEngine::DebugGui::CreateDebugGuiSystem;
    using AltinaEngine::DebugGui::DestroyDebugGuiSystem;

    const auto      tempRoot = std::filesystem::temp_directory_path() / "AltinaEditorUiAssetTest";
    std::error_code ec;
    std::filesystem::remove_all(tempRoot, ec);
    std::filesystem::create_directories(tempRoot / "SubFolder", ec);
    std::ofstream(tempRoot / "SubFolder" / "mesh.asset").put('x');
    std::ofstream(tempRoot / "SubFolder.meta").put('m');
    std::ofstream(tempRoot / "Root.meta").put('m');
    std::ofstream(tempRoot / "Texture.asset").put('t');

    auto* sys = CreateDebugGuiSystem();
    REQUIRE(sys != nullptr);
    sys->SetEnabled(true);
    sys->SetShowStats(false);
    sys->SetShowConsole(false);
    sys->SetShowCVars(false);

    AltinaEngine::Editor::UI::FEditorUiModule uiModule{};
    const auto                                rootString = ToFString(tempRoot);
    InitializeUi(uiModule, sys, rootString.ToView());

    AltinaEngine::Input::FInputSystem input;
    PrepareInput(input, 1280, 720, 320, 640);
    sys->TickGameThread(input, 1.0f / 60.0f, 1280, 720);
    TickUi(uiModule);

    const auto items =
        AltinaEngine::Editor::UI::Testing::FEditorUiTestingAccess::GetAssetItems(uiModule);
    REQUIRE(!items.IsEmpty());
    for (const auto& item : items) {
        REQUIRE(!item.ToView().EndsWith(TEXT(".meta")));
    }

    FString subFolderPath;
    for (const auto& item : items) {
        if (item.ToView().EndsWith(TEXT("SubFolder"))) {
            subFolderPath = item;
            break;
        }
    }
    REQUIRE(!subFolderPath.IsEmptyString());
    REQUIRE(AltinaEngine::Editor::UI::Testing::FEditorUiTestingAccess::OpenAssetPath(
        uiModule, subFolderPath.ToView(), AltinaEngine::Editor::UI::EAssetItemType::Directory));

    REQUIRE(AltinaEngine::Editor::UI::Testing::FEditorUiTestingAccess::GetCurrentAssetPath(uiModule)
            .ToView()
            .Contains(TEXT("SubFolder")));

    // Right click inside asset panel and trigger refresh menu item.
    PrepareInput(input, 1280, 720, 430, 585);
    input.OnMouseButtonDown(1U);
    sys->TickGameThread(input, 1.0f / 60.0f, 1280, 720);
    TickUi(uiModule);
    PrepareInput(input, 1280, 720, 430, 585);
    input.OnMouseButtonUp(1U);
    sys->TickGameThread(input, 1.0f / 60.0f, 1280, 720);
    TickUi(uiModule);
    PrepareInput(input, 1280, 720, 438, 612);
    input.OnMouseButtonDown(0U);
    sys->TickGameThread(input, 1.0f / 60.0f, 1280, 720);
    TickUi(uiModule);

    uiModule.Shutdown();
    DestroyDebugGuiSystem(sys);
    std::filesystem::remove_all(tempRoot, ec);
}

TEST_CASE("EditorUiModule play menu enqueues command and clear is caller-controlled") {
    using AltinaEngine::DebugGui::CreateDebugGuiSystem;
    using AltinaEngine::DebugGui::DestroyDebugGuiSystem;
    using AltinaEngine::Editor::UI::EEditorUiCommand;

    auto* sys = CreateDebugGuiSystem();
    REQUIRE(sys != nullptr);
    sys->SetEnabled(true);
    sys->SetShowStats(false);
    sys->SetShowConsole(false);
    sys->SetShowCVars(false);

    AltinaEngine::Editor::UI::FEditorUiModule uiModule{};
    InitializeUi(uiModule, sys);

    AltinaEngine::Input::FInputSystem input;

    // Click top-level "Play".
    PrepareInput(input, 1280, 720, 124, 8);
    input.OnMouseButtonDown(0U);
    sys->TickGameThread(input, 1.0f / 60.0f, 1280, 720);
    TickUi(uiModule);
    PrepareInput(input, 1280, 720, 124, 8);
    input.OnMouseButtonUp(0U);
    sys->TickGameThread(input, 1.0f / 60.0f, 1280, 720);
    TickUi(uiModule);

    // Click dropdown "Play" item.
    PrepareInput(input, 1280, 720, 128, 30);
    input.OnMouseButtonDown(0U);
    sys->TickGameThread(input, 1.0f / 60.0f, 1280, 720);

    auto output  = TickUi(uiModule, nullptr, true);
    bool hasPlay = false;
    for (auto cmd : output.mCommands) {
        if (cmd == EEditorUiCommand::Play) {
            hasPlay = true;
            break;
        }
    }
    REQUIRE(hasPlay);

    output = TickUi(uiModule, nullptr, true);
    REQUIRE(output.mCommands.IsEmpty());

    uiModule.Shutdown();
    DestroyDebugGuiSystem(sys);
}

TEST_CASE("EditorUiModule play menu disables unavailable commands by play state") {
    using AltinaEngine::DebugGui::CreateDebugGuiSystem;
    using AltinaEngine::DebugGui::DestroyDebugGuiSystem;
    using AltinaEngine::Editor::UI::EEditorUiCommand;
    using AltinaEngine::Editor::UI::EEditorUiPlayState;

    auto* sys = CreateDebugGuiSystem();
    REQUIRE(sys != nullptr);
    sys->SetEnabled(true);
    sys->SetShowStats(false);
    sys->SetShowConsole(false);
    sys->SetShowCVars(false);

    AltinaEngine::Editor::UI::FEditorUiModule uiModule{};
    InitializeUi(uiModule, sys);

    AltinaEngine::Input::FInputSystem input;

    auto tickWithState = [&](EEditorUiPlayState playState, AltinaEngine::i32 x, AltinaEngine::i32 y,
                             bool mouseDown, bool mouseUp) {
        PrepareInput(input, 1280, 720, x, y);
        if (mouseDown) {
            input.OnMouseButtonDown(0U);
        }
        if (mouseUp) {
            input.OnMouseButtonUp(0U);
        }
        sys->TickGameThread(input, 1.0f / 60.0f, 1280, 720);
        AltinaEngine::Editor::UI::FEditorUiFrameContext context{};
        context.bClearCommandBuffer = true;
        context.mPlayState          = playState;
        return uiModule.TickUi(context);
    };

    (void)tickWithState(EEditorUiPlayState::Running, 124, 8, true, false);
    (void)tickWithState(EEditorUiPlayState::Running, 124, 8, false, true);
    const auto runningOutput = tickWithState(EEditorUiPlayState::Running, 128, 30, true, false);
    bool       hasPlay       = false;
    for (auto cmd : runningOutput.mCommands) {
        if (cmd == EEditorUiCommand::Play) {
            hasPlay = true;
            break;
        }
    }
    REQUIRE(!hasPlay);

    uiModule.Shutdown();
    DestroyDebugGuiSystem(sys);
}

TEST_CASE("EditorUiModule hierarchy snapshot and inspector selection") {
    using AltinaEngine::Core::Container::FString;
    using AltinaEngine::Editor::UI::EEditorPropertyValueKind;
    using AltinaEngine::Editor::UI::EEditorSelectionType;
    using AltinaEngine::Editor::UI::FEditorComponentRuntimeId;
    using AltinaEngine::Editor::UI::FEditorGameObjectRuntimeId;
    using AltinaEngine::Editor::UI::FEditorPropertySnapshot;
    using AltinaEngine::Editor::UI::FEditorUiModule;
    using AltinaEngine::Editor::UI::FEditorWorldHierarchySnapshot;

    FEditorUiModule               uiModule{};
    FEditorWorldHierarchySnapshot snapshot{};
    snapshot.mWorldId = 99U;

    AltinaEngine::Editor::UI::FEditorGameObjectSnapshot root{};
    root.mId           = { 99U, 1U, 1U };
    root.mParentId     = {};
    root.mName         = FString(TEXT("Root"));
    root.bIsPrefabRoot = true;
    root.mComponents.PushBack({ { 101ULL, 1U, 1U }, FString(TEXT("CameraComponent")),
        FString(TEXT("CameraComponent")), FString(),
        { { FString(TEXT("nearPlane")), FString(TEXT("0.1")),
            EEditorPropertyValueKind::Scalar } } });

    AltinaEngine::Editor::UI::FEditorGameObjectSnapshot child{};
    child.mId       = { 99U, 2U, 1U };
    child.mParentId = { 99U, 1U, 1U };
    child.mName     = FString(TEXT("Child"));
    child.mComponents.PushBack({ { 202ULL, 2U, 1U }, FString(TEXT("ScriptComponent")),
        FString(TEXT("ScriptComponent")), FString(TEXT("AltinaEngine::GameScene")),
        { FEditorPropertySnapshot{ FString(TEXT("typeName")), FString(TEXT("PlayerController")),
            EEditorPropertyValueKind::String } } });

    AltinaEngine::Editor::UI::FEditorGameObjectSnapshot lone{};
    lone.mId       = { 99U, 3U, 1U };
    lone.mParentId = {};
    lone.mName     = FString(TEXT("Lone"));

    snapshot.mGameObjects.PushBack(root);
    snapshot.mGameObjects.PushBack(child);
    snapshot.mGameObjects.PushBack(lone);
    TickUi(uiModule, &snapshot);

    const auto hierarchyItems =
        AltinaEngine::Editor::UI::Testing::FEditorUiTestingAccess::GetHierarchyItems(uiModule);
    REQUIRE_EQ(hierarchyItems.Size(), 3U);
    REQUIRE(hierarchyItems[0].mLabel == TEXT("Root"));
    REQUIRE(hierarchyItems[0].mDepth == 0U);
    REQUIRE(!hierarchyItems[0].mIsComponent);
    REQUIRE(hierarchyItems[1].mLabel == TEXT("Child"));
    REQUIRE(hierarchyItems[1].mDepth == 1U);
    REQUIRE(!hierarchyItems[1].mIsComponent);
    REQUIRE(hierarchyItems[2].mLabel == TEXT("Lone"));
    REQUIRE(hierarchyItems[2].mDepth == 0U);
    REQUIRE(!hierarchyItems[2].mIsComponent);

    const auto roundTrippedSnapshot =
        AltinaEngine::Editor::UI::Testing::FEditorUiTestingAccess::GetHierarchySnapshot(uiModule);
    REQUIRE(roundTrippedSnapshot.mGameObjects[0].bIsPrefabRoot);
    REQUIRE_EQ(roundTrippedSnapshot.mGameObjects[0].mComponents[0].mProperties.Size(), 1U);
    REQUIRE(roundTrippedSnapshot.mGameObjects[0].mComponents[0].mProperties[0].mName
        == TEXT("nearPlane"));
    REQUIRE(roundTrippedSnapshot.mGameObjects[0].mComponents[0].mProperties[0].mDisplayValue
        == TEXT("0.1"));

    AltinaEngine::Editor::UI::Testing::FEditorUiTestingAccess::SelectGameObject(
        uiModule, FEditorGameObjectRuntimeId{ 99U, 2U, 1U });
    auto selection =
        AltinaEngine::Editor::UI::Testing::FEditorUiTestingAccess::GetSelectionInfo(uiModule);
    REQUIRE(selection.mType == EEditorSelectionType::GameObject);
    REQUIRE(selection.mName == TEXT("Child"));
    REQUIRE(selection.mUuid == TEXT("99-2-1"));

    AltinaEngine::Editor::UI::Testing::FEditorUiTestingAccess::SelectComponent(
        uiModule, FEditorComponentRuntimeId{ 202ULL, 2U, 1U });
    selection =
        AltinaEngine::Editor::UI::Testing::FEditorUiTestingAccess::GetSelectionInfo(uiModule);
    REQUIRE(selection.mType == EEditorSelectionType::Component);
    REQUIRE(selection.mName == TEXT("ScriptComponent"));
    REQUIRE(selection.mUuid == TEXT("202-2-1"));

    TickUi(uiModule, &snapshot);
    selection =
        AltinaEngine::Editor::UI::Testing::FEditorUiTestingAccess::GetSelectionInfo(uiModule);
    REQUIRE(selection.mType == EEditorSelectionType::None);
}

TEST_CASE("EditorUiModule hierarchy tolerates parent ids with missing world id") {
    using AltinaEngine::Core::Container::FString;
    using AltinaEngine::Editor::UI::FEditorUiModule;
    using AltinaEngine::Editor::UI::FEditorWorldHierarchySnapshot;

    FEditorUiModule               uiModule{};
    FEditorWorldHierarchySnapshot snapshot{};
    snapshot.mWorldId = 77U;

    AltinaEngine::Editor::UI::FEditorGameObjectSnapshot root{};
    root.mId       = { 77U, 10U, 1U };
    root.mParentId = {};
    root.mName     = FString(TEXT("RootA"));

    AltinaEngine::Editor::UI::FEditorGameObjectSnapshot child{};
    child.mId       = { 77U, 11U, 1U };
    child.mParentId = { 0U, 10U, 1U };
    child.mName     = FString(TEXT("ChildA"));

    AltinaEngine::Editor::UI::FEditorGameObjectSnapshot rootB{};
    rootB.mId       = { 77U, 20U, 1U };
    rootB.mParentId = {};
    rootB.mName     = FString(TEXT("RootB"));

    snapshot.mGameObjects.PushBack(root);
    snapshot.mGameObjects.PushBack(child);
    snapshot.mGameObjects.PushBack(rootB);

    TickUi(uiModule, &snapshot);
    const auto hierarchyItems =
        AltinaEngine::Editor::UI::Testing::FEditorUiTestingAccess::GetHierarchyItems(uiModule);
    REQUIRE_EQ(hierarchyItems.Size(), 3U);
    REQUIRE(hierarchyItems[0].mLabel == TEXT("RootA"));
    REQUIRE(hierarchyItems[0].mDepth == 0U);
    REQUIRE(hierarchyItems[1].mLabel == TEXT("ChildA"));
    REQUIRE(hierarchyItems[1].mDepth == 1U);
    REQUIRE(hierarchyItems[2].mLabel == TEXT("RootB"));
    REQUIRE(hierarchyItems[2].mDepth == 0U);
}

TEST_CASE("EditorUiModule hierarchy tolerates parent generation mismatch in snapshot") {
    using AltinaEngine::Core::Container::FString;
    using AltinaEngine::Editor::UI::FEditorUiModule;
    using AltinaEngine::Editor::UI::FEditorWorldHierarchySnapshot;

    FEditorUiModule               uiModule{};
    FEditorWorldHierarchySnapshot snapshot{};
    snapshot.mWorldId = 88U;

    AltinaEngine::Editor::UI::FEditorGameObjectSnapshot root{};
    root.mId       = { 88U, 100U, 7U };
    root.mParentId = {};
    root.mName     = FString(TEXT("Parent"));

    AltinaEngine::Editor::UI::FEditorGameObjectSnapshot child{};
    child.mId       = { 88U, 101U, 1U };
    child.mParentId = { 88U, 100U, 1U };
    child.mName     = FString(TEXT("Child"));

    snapshot.mGameObjects.PushBack(root);
    snapshot.mGameObjects.PushBack(child);

    TickUi(uiModule, &snapshot);
    const auto hierarchyItems =
        AltinaEngine::Editor::UI::Testing::FEditorUiTestingAccess::GetHierarchyItems(uiModule);
    REQUIRE_EQ(hierarchyItems.Size(), 2U);
    REQUIRE(hierarchyItems[0].mLabel == TEXT("Parent"));
    REQUIRE(hierarchyItems[0].mDepth == 0U);
    REQUIRE(hierarchyItems[1].mLabel == TEXT("Child"));
    REQUIRE(hierarchyItems[1].mDepth == 1U);
}

TEST_CASE("EditorUiModule hierarchy tolerates parent generation missing in snapshot") {
    using AltinaEngine::Core::Container::FString;
    using AltinaEngine::Editor::UI::FEditorUiModule;
    using AltinaEngine::Editor::UI::FEditorWorldHierarchySnapshot;

    FEditorUiModule               uiModule{};
    FEditorWorldHierarchySnapshot snapshot{};
    snapshot.mWorldId = 66U;

    AltinaEngine::Editor::UI::FEditorGameObjectSnapshot root{};
    root.mId       = { 66U, 30U, 9U };
    root.mParentId = {};
    root.mName     = FString(TEXT("RootG0"));

    AltinaEngine::Editor::UI::FEditorGameObjectSnapshot child{};
    child.mId       = { 66U, 31U, 1U };
    child.mParentId = { 66U, 30U, 0U };
    child.mName     = FString(TEXT("ChildG0"));

    snapshot.mGameObjects.PushBack(root);
    snapshot.mGameObjects.PushBack(child);

    TickUi(uiModule, &snapshot);
    const auto hierarchyItems =
        AltinaEngine::Editor::UI::Testing::FEditorUiTestingAccess::GetHierarchyItems(uiModule);
    REQUIRE_EQ(hierarchyItems.Size(), 2U);
    REQUIRE(hierarchyItems[0].mLabel == TEXT("RootG0"));
    REQUIRE(hierarchyItems[0].mDepth == 0U);
    REQUIRE(hierarchyItems[1].mLabel == TEXT("ChildG0"));
    REQUIRE(hierarchyItems[1].mDepth == 1U);
}

TEST_CASE("EditorUiModule viewport tab docking changes viewport request extent") {
    using AltinaEngine::DebugGui::CreateDebugGuiSystem;
    using AltinaEngine::DebugGui::DestroyDebugGuiSystem;

    auto* sys = CreateDebugGuiSystem();
    REQUIRE(sys != nullptr);
    sys->SetEnabled(true);
    sys->SetShowStats(false);
    sys->SetShowConsole(false);
    sys->SetShowCVars(false);

    AltinaEngine::Editor::UI::FEditorUiModule uiModule{};
    InitializeUi(uiModule, sys);

    AltinaEngine::Input::FInputSystem input;

    PrepareInput(input, 1280, 720, 280, 40);
    sys->TickGameThread(input, 1.0f / 60.0f, 1280, 720);
    auto beforeDock = TickUi(uiModule).mViewportRequest;
    REQUIRE(beforeDock.bHasContent);

    // Press viewport tab in center dock.
    PrepareInput(input, 1280, 720, 282, 40);
    input.OnMouseButtonDown(0U);
    sys->TickGameThread(input, 1.0f / 60.0f, 1280, 720);
    TickUi(uiModule);

    // Drag while holding.
    PrepareInput(input, 1280, 720, 120, 120);
    sys->TickGameThread(input, 1.0f / 60.0f, 1280, 720);
    TickUi(uiModule);

    // Release on left dock.
    PrepareInput(input, 1280, 720, 120, 120);
    input.OnMouseButtonUp(0U);
    sys->TickGameThread(input, 1.0f / 60.0f, 1280, 720);
    TickUi(uiModule);

    PrepareInput(input, 1280, 720, 120, 120);
    sys->TickGameThread(input, 1.0f / 60.0f, 1280, 720);
    const auto afterDock = TickUi(uiModule).mViewportRequest;
    REQUIRE(afterDock.bHasContent);
    REQUIRE(afterDock.Width > 0U);
    REQUIRE(afterDock.Width < beforeDock.Width);

    uiModule.Shutdown();
    DestroyDebugGuiSystem(sys);
}

TEST_CASE("EditorUiModule inspector collapse toggles and handles empty states") {
    using AltinaEngine::Core::Container::FString;
    using AltinaEngine::DebugGui::CreateDebugGuiSystem;
    using AltinaEngine::DebugGui::DestroyDebugGuiSystem;
    using AltinaEngine::Editor::UI::EEditorPropertyValueKind;
    using AltinaEngine::Editor::UI::FEditorComponentRuntimeId;
    using AltinaEngine::Editor::UI::FEditorUiModule;
    using AltinaEngine::Editor::UI::FEditorWorldHierarchySnapshot;

    auto* sys = CreateDebugGuiSystem();
    REQUIRE(sys != nullptr);
    sys->SetEnabled(true);
    sys->SetShowStats(false);
    sys->SetShowConsole(false);
    sys->SetShowCVars(false);

    FEditorUiModule uiModule{};
    InitializeUi(uiModule, sys);

    FEditorWorldHierarchySnapshot snapshot{};
    snapshot.mWorldId = 7U;

    AltinaEngine::Editor::UI::FEditorGameObjectSnapshot object{};
    object.mId           = { 7U, 1U, 1U };
    object.mName         = FString(TEXT("InspectorTarget"));
    object.bIsPrefabRoot = false;
    object.mComponents.PushBack({ { 1001ULL, 2U, 1U }, FString(TEXT("CameraComponent")),
        FString(TEXT("CameraComponent")), FString(),
        { { FString(TEXT("nearPlane")), FString(TEXT("0.1")), EEditorPropertyValueKind::Scalar },
            { FString(TEXT("farPlane")), FString(TEXT("1000")),
                EEditorPropertyValueKind::Scalar } } });
    snapshot.mGameObjects.PushBack(object);

    AltinaEngine::Input::FInputSystem input;
    PrepareInput(input, 1280, 720, 1100, 200);
    sys->TickGameThread(input, 1.0f / 60.0f, 1280, 720);
    TickUi(uiModule, &snapshot);

    AltinaEngine::Editor::UI::Testing::FEditorUiTestingAccess::SelectGameObject(
        uiModule, { 7U, 1U, 1U });

    PrepareInput(input, 1280, 720, 1100, 200);
    sys->TickGameThread(input, 1.0f / 60.0f, 1280, 720);
    TickUi(uiModule, &snapshot);

    const auto layout      = BuildInspectorTestLayout(sys, 1280U, 720U);
    const auto componentId = FEditorComponentRuntimeId{ 1001ULL, 2U, 1U };
    REQUIRE(
        !AltinaEngine::Editor::UI::Testing::FEditorUiTestingAccess::IsInspectorComponentExpanded(
            uiModule, componentId));

    PrepareInput(input, 1280, 720, layout.mFirstComponentHeaderX, layout.mFirstComponentHeaderY);
    input.OnMouseButtonDown(0U);
    sys->TickGameThread(input, 1.0f / 60.0f, 1280, 720);
    TickUi(uiModule, &snapshot);

    PrepareInput(input, 1280, 720, layout.mFirstComponentHeaderX, layout.mFirstComponentHeaderY);
    input.OnMouseButtonUp(0U);
    sys->TickGameThread(input, 1.0f / 60.0f, 1280, 720);
    TickUi(uiModule, &snapshot);

    REQUIRE(AltinaEngine::Editor::UI::Testing::FEditorUiTestingAccess::IsInspectorComponentExpanded(
        uiModule, componentId));

    FEditorWorldHierarchySnapshot emptySnapshot{};
    PrepareInput(input, 1280, 720, 1100, 200);
    sys->TickGameThread(input, 1.0f / 60.0f, 1280, 720);
    TickUi(uiModule, &emptySnapshot);

    const auto selection =
        AltinaEngine::Editor::UI::Testing::FEditorUiTestingAccess::GetSelectionInfo(uiModule);
    REQUIRE(selection.mType == AltinaEngine::Editor::UI::EEditorSelectionType::None);

    uiModule.Shutdown();
    DestroyDebugGuiSystem(sys);
}

TEST_CASE("EditorUiModule inspector scrolls for large component lists") {
    using AltinaEngine::Core::Container::FString;
    using AltinaEngine::DebugGui::CreateDebugGuiSystem;
    using AltinaEngine::DebugGui::DestroyDebugGuiSystem;
    using AltinaEngine::Editor::UI::EEditorPropertyValueKind;
    using AltinaEngine::Editor::UI::FEditorUiModule;
    using AltinaEngine::Editor::UI::FEditorWorldHierarchySnapshot;

    auto* sys = CreateDebugGuiSystem();
    REQUIRE(sys != nullptr);
    sys->SetEnabled(true);
    sys->SetShowStats(false);
    sys->SetShowConsole(false);
    sys->SetShowCVars(false);

    FEditorUiModule uiModule{};
    InitializeUi(uiModule, sys);

    FEditorWorldHierarchySnapshot snapshot{};
    snapshot.mWorldId = 9U;
    AltinaEngine::Editor::UI::FEditorGameObjectSnapshot object{};
    object.mId   = { 9U, 1U, 1U };
    object.mName = FString(TEXT("ManyComponents"));
    for (AltinaEngine::u32 index = 0U; index < 12U; ++index) {
        FString componentName(TEXT("Component"));
        componentName.AppendNumber(static_cast<AltinaEngine::i32>(index));
        AltinaEngine::Editor::UI::FEditorComponentSnapshot component{};
        component.mId       = { 3000ULL + index, index + 1U, 1U };
        component.mName     = componentName;
        component.mTypeName = componentName;
        for (AltinaEngine::u32 propIndex = 0U; propIndex < 4U; ++propIndex) {
            FString propName(TEXT("Value"));
            propName.AppendNumber(static_cast<AltinaEngine::i32>(propIndex));
            FString propValue(TEXT("Text"));
            propValue.AppendNumber(static_cast<AltinaEngine::i32>(propIndex));
            component.mProperties.PushBack(
                { propName, propValue, EEditorPropertyValueKind::String });
        }
        object.mComponents.PushBack(component);
    }
    snapshot.mGameObjects.PushBack(object);

    AltinaEngine::Editor::UI::Testing::FEditorUiTestingAccess::SelectGameObject(
        uiModule, { 9U, 1U, 1U });

    AltinaEngine::Input::FInputSystem input;
    PrepareInput(input, 1280, 720, 1100, 200);
    sys->TickGameThread(input, 1.0f / 60.0f, 1280, 720);
    TickUi(uiModule, &snapshot);

    AltinaEngine::Editor::UI::Testing::FEditorUiTestingAccess::SetInspectorScrollY(
        uiModule, 180.0f);
    PrepareInput(input, 1280, 720, 1100, 260);
    sys->TickGameThread(input, 1.0f / 60.0f, 1280, 720);
    TickUi(uiModule, &snapshot);

    REQUIRE(AltinaEngine::Editor::UI::Testing::FEditorUiTestingAccess::GetInspectorScrollY(uiModule)
        > 0.0f);

    uiModule.Shutdown();
    DestroyDebugGuiSystem(sys);
}

TEST_CASE("EditorUiModule initialize and shutdown toggles module state") {
    using AltinaEngine::DebugGui::CreateDebugGuiSystem;
    using AltinaEngine::DebugGui::DestroyDebugGuiSystem;

    auto* sys = CreateDebugGuiSystem();
    REQUIRE(sys != nullptr);
    sys->SetEnabled(true);

    AltinaEngine::Editor::UI::FEditorUiModule uiModule{};
    REQUIRE(!uiModule.IsInitialized());
    InitializeUi(uiModule, sys);
    REQUIRE(uiModule.IsInitialized());
    uiModule.Shutdown();
    REQUIRE(!uiModule.IsInitialized());

    DestroyDebugGuiSystem(sys);
}

TEST_CASE("EditorUiModule menu open blocks runtime viewport input") {
    using AltinaEngine::DebugGui::CreateDebugGuiSystem;
    using AltinaEngine::DebugGui::DestroyDebugGuiSystem;

    auto* sys = CreateDebugGuiSystem();
    REQUIRE(sys != nullptr);
    sys->SetEnabled(true);
    sys->SetShowStats(false);
    sys->SetShowConsole(false);
    sys->SetShowCVars(false);

    AltinaEngine::Editor::UI::FEditorUiModule uiModule{};
    InitializeUi(uiModule, sys);

    AltinaEngine::Input::FInputSystem input;
    PrepareInput(input, 1280, 720, 10, 8);
    input.OnMouseButtonDown(0U);
    sys->TickGameThread(input, 1.0f / 60.0f, 1280, 720);

    const auto output = TickUi(uiModule);
    REQUIRE(output.mViewportRequest.bUiBlockingInput);

    uiModule.Shutdown();
    DestroyDebugGuiSystem(sys);
}
