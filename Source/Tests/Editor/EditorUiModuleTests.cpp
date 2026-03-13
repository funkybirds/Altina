#include "TestHarness.h"

#include "EditorUI/EditorUiModule.h"
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
    uiModule.RegisterDefaultPanels(sys);

    AltinaEngine::Input::FInputSystem input;
    PrepareInput(input, 1280, 720, 300, 200);
    sys->TickGameThread(input, 1.0f / 60.0f, 1280, 720);

    const auto request = uiModule.GetViewportRequest();
    REQUIRE(request.bHasContent);
    REQUIRE(request.Width > 0U);
    REQUIRE(request.Height > 0U);

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
    uiModule.RegisterDefaultPanels(sys, rootString.ToView());

    AltinaEngine::Input::FInputSystem input;
    PrepareInput(input, 1280, 720, 320, 640);
    sys->TickGameThread(input, 1.0f / 60.0f, 1280, 720);

    const auto items = uiModule.DebugGetAssetItemsForTest();
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
    REQUIRE(uiModule.DebugOpenAssetPathForTest(
        subFolderPath.ToView(), AltinaEngine::Editor::UI::EAssetItemType::Directory));

    REQUIRE(uiModule.DebugGetCurrentAssetPathForTest().ToView().Contains(TEXT("SubFolder")));

    // Right click inside asset panel and trigger refresh menu item.
    PrepareInput(input, 1280, 720, 430, 585);
    input.OnMouseButtonDown(1U);
    sys->TickGameThread(input, 1.0f / 60.0f, 1280, 720);
    PrepareInput(input, 1280, 720, 430, 585);
    input.OnMouseButtonUp(1U);
    sys->TickGameThread(input, 1.0f / 60.0f, 1280, 720);
    PrepareInput(input, 1280, 720, 438, 612);
    input.OnMouseButtonDown(0U);
    sys->TickGameThread(input, 1.0f / 60.0f, 1280, 720);

    DestroyDebugGuiSystem(sys);
    std::filesystem::remove_all(tempRoot, ec);
}

TEST_CASE("EditorUiModule play menu enqueues command and consume clears queue") {
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
    uiModule.RegisterDefaultPanels(sys);

    AltinaEngine::Input::FInputSystem input;

    // Click top-level "Play".
    PrepareInput(input, 1280, 720, 124, 8);
    input.OnMouseButtonDown(0U);
    sys->TickGameThread(input, 1.0f / 60.0f, 1280, 720);
    PrepareInput(input, 1280, 720, 124, 8);
    input.OnMouseButtonUp(0U);
    sys->TickGameThread(input, 1.0f / 60.0f, 1280, 720);

    // Click dropdown "Play" item.
    PrepareInput(input, 1280, 720, 128, 30);
    input.OnMouseButtonDown(0U);
    sys->TickGameThread(input, 1.0f / 60.0f, 1280, 720);

    auto commands = uiModule.ConsumeUiCommands();
    bool hasPlay  = false;
    for (auto cmd : commands) {
        if (cmd == EEditorUiCommand::Play) {
            hasPlay = true;
            break;
        }
    }
    REQUIRE(hasPlay);

    commands = uiModule.ConsumeUiCommands();
    REQUIRE(commands.IsEmpty());

    DestroyDebugGuiSystem(sys);
}

TEST_CASE("EditorUiModule hierarchy snapshot and inspector selection") {
    using AltinaEngine::Core::Container::FString;
    using AltinaEngine::Editor::UI::EEditorSelectionType;
    using AltinaEngine::Editor::UI::FEditorComponentRuntimeId;
    using AltinaEngine::Editor::UI::FEditorGameObjectRuntimeId;
    using AltinaEngine::Editor::UI::FEditorUiModule;
    using AltinaEngine::Editor::UI::FEditorWorldHierarchySnapshot;

    FEditorUiModule               uiModule{};
    FEditorWorldHierarchySnapshot snapshot{};
    snapshot.mWorldId = 99U;

    AltinaEngine::Editor::UI::FEditorGameObjectSnapshot root{};
    root.mId       = { 99U, 1U, 1U };
    root.mParentId = {};
    root.mName     = FString(TEXT("Root"));
    root.mComponents.PushBack({ { 101ULL, 1U, 1U }, FString(TEXT("CameraComponent")) });

    AltinaEngine::Editor::UI::FEditorGameObjectSnapshot child{};
    child.mId       = { 99U, 2U, 1U };
    child.mParentId = { 99U, 1U, 1U };
    child.mName     = FString(TEXT("Child"));
    child.mComponents.PushBack({ { 202ULL, 2U, 1U }, FString(TEXT("ScriptComponent")) });

    AltinaEngine::Editor::UI::FEditorGameObjectSnapshot lone{};
    lone.mId       = { 99U, 3U, 1U };
    lone.mParentId = {};
    lone.mName     = FString(TEXT("Lone"));

    snapshot.mGameObjects.PushBack(root);
    snapshot.mGameObjects.PushBack(child);
    snapshot.mGameObjects.PushBack(lone);
    uiModule.SetWorldHierarchySnapshot(snapshot);

    const auto hierarchyItems = uiModule.DebugGetHierarchyItemsForTest();
    REQUIRE_EQ(hierarchyItems.Size(), 5U);
    REQUIRE(hierarchyItems[0].mLabel == TEXT("Root"));
    REQUIRE(hierarchyItems[0].mDepth == 0U);
    REQUIRE(!hierarchyItems[0].mIsComponent);
    REQUIRE(hierarchyItems[1].mLabel == TEXT("CameraComponent"));
    REQUIRE(hierarchyItems[1].mDepth == 1U);
    REQUIRE(hierarchyItems[1].mIsComponent);
    REQUIRE(hierarchyItems[2].mLabel == TEXT("Child"));
    REQUIRE(hierarchyItems[2].mDepth == 1U);
    REQUIRE(!hierarchyItems[2].mIsComponent);
    REQUIRE(hierarchyItems[3].mLabel == TEXT("ScriptComponent"));
    REQUIRE(hierarchyItems[3].mDepth == 2U);
    REQUIRE(hierarchyItems[3].mIsComponent);
    REQUIRE(hierarchyItems[4].mLabel == TEXT("Lone"));
    REQUIRE(hierarchyItems[4].mDepth == 0U);
    REQUIRE(!hierarchyItems[4].mIsComponent);

    uiModule.DebugSelectGameObjectForTest(FEditorGameObjectRuntimeId{ 99U, 2U, 1U });
    auto selection = uiModule.DebugGetSelectionInfoForTest();
    REQUIRE(selection.mType == EEditorSelectionType::GameObject);
    REQUIRE(selection.mName == TEXT("Child"));
    REQUIRE(selection.mUuid == TEXT("99-2-1"));

    uiModule.DebugSelectComponentForTest(FEditorComponentRuntimeId{ 202ULL, 2U, 1U });
    selection = uiModule.DebugGetSelectionInfoForTest();
    REQUIRE(selection.mType == EEditorSelectionType::Component);
    REQUIRE(selection.mName == TEXT("ScriptComponent"));
    REQUIRE(selection.mUuid == TEXT("202-2-1"));

    uiModule.SetWorldHierarchySnapshot(FEditorWorldHierarchySnapshot{});
    selection = uiModule.DebugGetSelectionInfoForTest();
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

    uiModule.SetWorldHierarchySnapshot(snapshot);
    const auto hierarchyItems = uiModule.DebugGetHierarchyItemsForTest();
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

    uiModule.SetWorldHierarchySnapshot(snapshot);
    const auto hierarchyItems = uiModule.DebugGetHierarchyItemsForTest();
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

    uiModule.SetWorldHierarchySnapshot(snapshot);
    const auto hierarchyItems = uiModule.DebugGetHierarchyItemsForTest();
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
    uiModule.RegisterDefaultPanels(sys);

    AltinaEngine::Input::FInputSystem input;

    PrepareInput(input, 1280, 720, 280, 40);
    sys->TickGameThread(input, 1.0f / 60.0f, 1280, 720);
    const auto beforeDock = uiModule.GetViewportRequest();
    REQUIRE(beforeDock.bHasContent);

    // Press viewport tab in center dock.
    PrepareInput(input, 1280, 720, 282, 40);
    input.OnMouseButtonDown(0U);
    sys->TickGameThread(input, 1.0f / 60.0f, 1280, 720);

    // Drag while holding.
    PrepareInput(input, 1280, 720, 120, 120);
    sys->TickGameThread(input, 1.0f / 60.0f, 1280, 720);

    // Release on left dock.
    PrepareInput(input, 1280, 720, 120, 120);
    input.OnMouseButtonUp(0U);
    sys->TickGameThread(input, 1.0f / 60.0f, 1280, 720);

    PrepareInput(input, 1280, 720, 120, 120);
    sys->TickGameThread(input, 1.0f / 60.0f, 1280, 720);
    const auto afterDock = uiModule.GetViewportRequest();
    REQUIRE(afterDock.bHasContent);
    REQUIRE(afterDock.Width > 0U);
    REQUIRE(afterDock.Width < beforeDock.Width);

    DestroyDebugGuiSystem(sys);
}
