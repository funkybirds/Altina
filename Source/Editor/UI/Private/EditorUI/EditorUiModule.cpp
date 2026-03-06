#include "EditorUI/EditorUiModule.h"

#include "DebugGui/DebugGui.h"
#include "Math/Vector.h"

namespace AltinaEngine::Editor::UI {
    void FEditorUiModule::RegisterDefaultPanels(DebugGui::IDebugGuiSystem* debugGuiSystem) const {
        if (debugGuiSystem == nullptr) {
            return;
        }

        debugGuiSystem->RegisterOverlay(
            TEXT("Editor.UI.DefaultOverlay"), [](DebugGui::IDebugGui& gui) {
                constexpr auto kColor = DebugGui::MakeColor32(220, 220, 220, 255);
                gui.DrawText(Core::Math::FVector2f(14.0f, 62.0f), kColor,
                    TEXT("Editor Modules: Core/UI/Viewport/PlaySession"));
            });
    }
} // namespace AltinaEngine::Editor::UI
