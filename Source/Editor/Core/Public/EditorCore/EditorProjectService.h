#pragma once

#include "Base/EditorCoreAPI.h"
#include "Container/String.h"
#include "Container/StringView.h"

namespace AltinaEngine::Editor::Core {
    using AltinaEngine::Core::Container::FString;
    using AltinaEngine::Core::Container::FStringView;

    struct FEditorProjectSettings {
        bool    bLoaded = false;
        FString DemoName;
        FString DemoModulePath;
        FString ConfigOverride;
        FString AssetRootOverride;
        FString SourcePath;
    };

    class AE_EDITOR_CORE_API FEditorProjectService final {
    public:
        auto LoadFromCommandLine(FStringView commandLine, FEditorProjectSettings& outSettings) const
            -> bool;
    };
} // namespace AltinaEngine::Editor::Core
