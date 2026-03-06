#pragma once

#include "Base/EditorViewportAPI.h"

namespace AltinaEngine::Launch {
    class IRuntimeSession;
}

namespace AltinaEngine::Editor::Viewport {
    class AE_EDITOR_VIEWPORT_API FEditorViewportBootstrap final {
    public:
        void EnsureDefaultWorld(Launch::IRuntimeSession& session) const;
    };
} // namespace AltinaEngine::Editor::Viewport
