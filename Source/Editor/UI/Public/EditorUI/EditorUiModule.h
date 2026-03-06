#pragma once

#include "Base/EditorUIAPI.h"

namespace AltinaEngine::DebugGui {
    class IDebugGuiSystem;
}

namespace AltinaEngine::Editor::UI {
    class AE_EDITOR_UI_API FEditorUiModule final {
    public:
        void RegisterDefaultPanels(DebugGui::IDebugGuiSystem* debugGuiSystem) const;
    };
} // namespace AltinaEngine::Editor::UI
