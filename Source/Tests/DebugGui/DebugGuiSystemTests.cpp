#include "TestHarness.h"

#include "CoreMinimal.h"

#include "DebugGui/DebugGui.h"

#include "Console/ConsoleVariable.h"

#include "Input/InputSystem.h"
#include "Input/Keys.h"

using AltinaEngine::Core::Math::FVector2f;
using AltinaEngine::DebugGui::CreateDebugGuiSystem;
using AltinaEngine::DebugGui::DestroyDebugGuiSystem;
using AltinaEngine::DebugGui::FRect;
using AltinaEngine::DebugGui::IDebugGui;
using AltinaEngine::DebugGui::IDebugGuiSystem;
using AltinaEngine::DebugGui::MakeColor32;

namespace {
    void PrepareInput(AltinaEngine::Input::FInputSystem& input, AltinaEngine::u32 w,
        AltinaEngine::u32 h, AltinaEngine::i32 mx, AltinaEngine::i32 my) {
        input.ClearFrameState();
        input.OnWindowResized(w, h);
        input.OnWindowFocusGained();
        input.OnMouseMove(mx, my);
    }
} // namespace

TEST_CASE("DebugGui builds overlay when enabled") {
    IDebugGuiSystem* sys = CreateDebugGuiSystem();
    REQUIRE(sys != nullptr);
    sys->SetEnabled(true);

    AltinaEngine::Input::FInputSystem input;
    PrepareInput(input, 1280, 720, 50, 50);

    sys->TickGameThread(input, 1.0f / 60.0f, 1280, 720);
    const auto stats = sys->GetLastFrameStats();
    REQUIRE(stats.VertexCount > 0U);
    REQUIRE(stats.IndexCount > 0U);
    REQUIRE(stats.CmdCount > 0U);

    DestroyDebugGuiSystem(sys);
}

TEST_CASE("DebugGui panel drawing increases draw stats") {
    IDebugGuiSystem* sys = CreateDebugGuiSystem();
    REQUIRE(sys != nullptr);
    sys->SetEnabled(true);

    AltinaEngine::Input::FInputSystem input;
    PrepareInput(input, 640, 480, 10, 10);

    sys->TickGameThread(input, 1.0f / 60.0f, 640, 480);
    const auto baseline = sys->GetLastFrameStats();

    sys->RegisterPanel(TEXT("TestPanel"), [](IDebugGui& gui) {
        const FRect clip{ FVector2f(0.0f, 0.0f), FVector2f(64.0f, 64.0f) };
        gui.PushClipRect(clip);
        gui.DrawRectFilled(clip, MakeColor32(255, 0, 0, 255));
        gui.DrawText(FVector2f(4.0f, 4.0f), MakeColor32(255, 255, 255, 255), TEXT("ABC123"));
        gui.PopClipRect();
    });

    PrepareInput(input, 640, 480, 10, 10);
    sys->TickGameThread(input, 1.0f / 60.0f, 640, 480);
    const auto withPanel = sys->GetLastFrameStats();

    REQUIRE(withPanel.VertexCount > baseline.VertexCount);
    REQUIRE(withPanel.IndexCount > baseline.IndexCount);
    REQUIRE(withPanel.CmdCount >= baseline.CmdCount);

    DestroyDebugGuiSystem(sys);
}

TEST_CASE("DebugGui toggles off with F1") {
    IDebugGuiSystem* sys = CreateDebugGuiSystem();
    REQUIRE(sys != nullptr);
    sys->SetEnabled(true);

    AltinaEngine::Input::FInputSystem input;
    PrepareInput(input, 800, 600, 10, 10);

    // Press F1 this frame.
    input.OnKeyDown(AltinaEngine::Input::EKey::F1, false);

    sys->TickGameThread(input, 1.0f / 60.0f, 800, 600);
    // Next frame should be disabled (no draw).
    PrepareInput(input, 800, 600, 10, 10);
    sys->TickGameThread(input, 1.0f / 60.0f, 800, 600);
    const auto stats = sys->GetLastFrameStats();
    REQUIRE_EQ(stats.VertexCount, 0U);
    REQUIRE_EQ(stats.IndexCount, 0U);

    DestroyDebugGuiSystem(sys);
}

TEST_CASE("DebugGui widgets: Button/Checkbox/Slider/InputText basic interactions") {
    IDebugGuiSystem* sys = CreateDebugGuiSystem();
    REQUIRE(sys != nullptr);
    sys->SetEnabled(true);

    // Use a large height so the custom window (stacked after built-ins) remains visible.
    constexpr AltinaEngine::u32            kW = 1280U;
    constexpr AltinaEngine::u32            kH = 2000U;

    bool                                   clicked = false;
    bool                                   check   = false;
    float                                  slider  = 0.0f;
    AltinaEngine::Core::Container::FString text;

    sys->RegisterPanel(TEXT("WidgetTest"), [&](IDebugGui& gui) {
        if (gui.Button(TEXT("ClickMe"))) {
            clicked = true;
        }
        (void)gui.Checkbox(TEXT("CheckMe"), check);
        (void)gui.SliderFloat(TEXT("SlideMe"), slider, 0.0f, 1.0f);
        (void)gui.InputText(TEXT("TextMe"), text);
    });

    AltinaEngine::Input::FInputSystem input;

    // Custom panel window index is 3 (Stats/Console/CVars are drawn before it).
    const AltinaEngine::i32           winY     = 10 + 3 * (260 + 10);
    const AltinaEngine::i32           contentY = winY + 18 + 8;
    const AltinaEngine::i32           contentX = 10 + 8;

    // Button click (press then release).
    PrepareInput(input, kW, kH, contentX + 10, contentY + 5);
    input.OnMouseButtonDown(0U);
    sys->TickGameThread(input, 1.0f / 60.0f, kW, kH);

    PrepareInput(input, kW, kH, contentX + 10, contentY + 5);
    input.OnMouseButtonUp(0U);
    sys->TickGameThread(input, 1.0f / 60.0f, kW, kH);
    REQUIRE(clicked);

    // Checkbox toggles on click.
    clicked = false;
    PrepareInput(input, kW, kH, contentX + 10, contentY + 22);
    input.OnMouseButtonDown(0U);
    sys->TickGameThread(input, 1.0f / 60.0f, kW, kH);

    PrepareInput(input, kW, kH, contentX + 10, contentY + 22);
    input.OnMouseButtonUp(0U);
    sys->TickGameThread(input, 1.0f / 60.0f, kW, kH);
    REQUIRE(check);

    // Slider drag roughly to the right.
    const AltinaEngine::i32 sliderY = contentY + 50;
    PrepareInput(input, kW, kH, contentX + 5, sliderY);
    input.OnMouseButtonDown(0U);
    sys->TickGameThread(input, 1.0f / 60.0f, kW, kH);

    PrepareInput(input, kW, kH, contentX + 430, sliderY);
    sys->TickGameThread(input, 1.0f / 60.0f, kW, kH);

    PrepareInput(input, kW, kH, contentX + 430, sliderY);
    input.OnMouseButtonUp(0U);
    sys->TickGameThread(input, 1.0f / 60.0f, kW, kH);
    REQUIRE(slider > 0.7f);

    // InputText: click to focus, type 'abc'.
    const AltinaEngine::i32 inputY = contentY + 88;
    PrepareInput(input, kW, kH, contentX + 10, inputY);
    input.OnMouseButtonDown(0U);
    sys->TickGameThread(input, 1.0f / 60.0f, kW, kH);

    PrepareInput(input, kW, kH, contentX + 10, inputY);
    input.OnMouseButtonUp(0U);
    input.OnCharInput(static_cast<AltinaEngine::u32>('a'));
    input.OnCharInput(static_cast<AltinaEngine::u32>('b'));
    input.OnCharInput(static_cast<AltinaEngine::u32>('c'));
    sys->TickGameThread(input, 1.0f / 60.0f, kW, kH);
    REQUIRE(text.ToView().Contains(TEXT("abc")));

    DestroyDebugGuiSystem(sys);
}

TEST_CASE("DebugGui console executes 'set' command via Enter") {
    IDebugGuiSystem* sys = CreateDebugGuiSystem();
    REQUIRE(sys != nullptr);
    sys->SetEnabled(true);

    auto* cvar =
        AltinaEngine::Core::Console::FConsoleVariable::Register(TEXT("DebugGui.TestInt"), 1);
    REQUIRE(cvar != nullptr);

    AltinaEngine::Input::FInputSystem input;
    constexpr AltinaEngine::u32       kW = 1280U;
    constexpr AltinaEngine::u32       kH = 2000U;

    // Console window index is 1 (Stats is 0).
    const AltinaEngine::i32           winY        = 10 + 1 * (260 + 10);
    const AltinaEngine::i32           contentY    = winY + 18 + 8;
    const AltinaEngine::i32           contentX    = 10 + 8;
    const AltinaEngine::i32           commandBoxY = contentY + 78; // inside command box

    // Focus command input.
    PrepareInput(input, kW, kH, contentX + 10, commandBoxY);
    input.OnMouseButtonDown(0U);
    sys->TickGameThread(input, 1.0f / 60.0f, kW, kH);

    PrepareInput(input, kW, kH, contentX + 10, commandBoxY);
    input.OnMouseButtonUp(0U);
    const char* cmd = "set DebugGui.TestInt 123";
    for (const char* p = cmd; *p != '\0'; ++p) {
        input.OnCharInput(static_cast<AltinaEngine::u32>(*p));
    }
    sys->TickGameThread(input, 1.0f / 60.0f, kW, kH);

    PrepareInput(input, kW, kH, contentX + 10, commandBoxY);
    input.OnKeyDown(AltinaEngine::Input::EKey::Enter, false);
    sys->TickGameThread(input, 1.0f / 60.0f, kW, kH);

    REQUIRE_EQ(cvar->GetValue<int>(), 123);

    DestroyDebugGuiSystem(sys);
}
