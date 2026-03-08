#include "TestHarness.h"

#include "EditorUI/EditorUiModule.h"
#include "DebugGui/DebugGui.h"
#include "Input/InputSystem.h"

namespace {
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
