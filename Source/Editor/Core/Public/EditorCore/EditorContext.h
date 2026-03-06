#pragma once

#include "Base/EditorCoreAPI.h"
#include "CoreMinimal.h"

namespace AltinaEngine::Editor::Core {
    struct FEditorContext {
        bool bInitialized = false;
        u64  FrameIndex   = 0ULL;
    };

    class AE_EDITOR_CORE_API FEditorCoreModule final {
    public:
        auto Initialize(FEditorContext& context) -> bool;
        void Shutdown(FEditorContext& context);
    };
} // namespace AltinaEngine::Editor::Core
