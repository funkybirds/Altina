#pragma once

#include "EditorUI/EditorUiModule.h"

namespace AltinaEngine::Editor::UI::Testing {
    class FEditorUiTestingAccess final {
    public:
        [[nodiscard]] static auto GetAssetItems(const FEditorUiModule& module)
            -> ::AltinaEngine::Core::Container::TVector<::AltinaEngine::Core::Container::FString> {
            return module.DebugGetAssetItemsForTest();
        }

        [[nodiscard]] static auto GetCurrentAssetPath(const FEditorUiModule& module)
            -> ::AltinaEngine::Core::Container::FString {
            return module.DebugGetCurrentAssetPathForTest();
        }

        [[nodiscard]] static auto GetHierarchyItems(const FEditorUiModule& module)
            -> ::AltinaEngine::Core::Container::TVector<FEditorHierarchyDebugItem> {
            return module.DebugGetHierarchyItemsForTest();
        }

        [[nodiscard]] static auto GetSelectionInfo(const FEditorUiModule& module)
            -> FEditorSelectionInfo {
            return module.DebugGetSelectionInfoForTest();
        }

        static void SelectGameObject(FEditorUiModule& module, FEditorGameObjectRuntimeId id) {
            module.DebugSelectGameObjectForTest(id);
        }

        static void SelectComponent(FEditorUiModule& module, FEditorComponentRuntimeId id) {
            module.DebugSelectComponentForTest(id);
        }

        [[nodiscard]] static auto OpenAssetPath(FEditorUiModule& module,
            ::AltinaEngine::Core::Container::FStringView path, EAssetItemType type) -> bool {
            return module.DebugOpenAssetPathForTest(path, type);
        }
    };
} // namespace AltinaEngine::Editor::UI::Testing
