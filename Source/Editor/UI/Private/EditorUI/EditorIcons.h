#pragma once

#include "Container/StringView.h"
#include "DebugGui/Core/Types.h"

namespace AltinaEngine::Editor::UI::Private {
    enum class EEditorIconId : DebugGui::FDebugGuiIconId {
        None               = 0U,
        PanelHierarchy     = 1U,
        PanelViewport      = 2U,
        PanelInspector     = 3U,
        PanelAsset         = 4U,
        PanelOutput        = 5U,
        AssetFolder        = 6U,
        AssetFile          = 7U,
        AssetNavigateUp    = 8U,
        ComponentGeneric   = 9U,
        ComponentTransform = 10U,
        ComponentCamera    = 11U,
        ComponentLight     = 12U,
        ComponentMesh      = 13U,
        ComponentScript    = 14U,
        ComponentPhysics   = 15U,
        GameObject         = 16U
    };

    [[nodiscard]] auto ToIconId(EEditorIconId icon) noexcept -> DebugGui::FDebugGuiIconId;
    [[nodiscard]] auto GetPanelIconId(Core::Container::FStringView panelTitle) noexcept
        -> DebugGui::FDebugGuiIconId;
    [[nodiscard]] auto GetAssetIconId(bool bNavigateUp, bool bDirectory) noexcept
        -> DebugGui::FDebugGuiIconId;
    [[nodiscard]] auto GetComponentIconId(Core::Container::FStringView componentName,
        Core::Container::FStringView componentTypeName) -> DebugGui::FDebugGuiIconId;
} // namespace AltinaEngine::Editor::UI::Private
