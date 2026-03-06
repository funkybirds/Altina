#include "EditorCore/EditorContext.h"

namespace AltinaEngine::Editor::Core {
    auto FEditorCoreModule::Initialize(FEditorContext& context) -> bool {
        context.bInitialized = true;
        context.FrameIndex   = 0ULL;
        return true;
    }

    void FEditorCoreModule::Shutdown(FEditorContext& context) { context.bInitialized = false; }
} // namespace AltinaEngine::Editor::Core
