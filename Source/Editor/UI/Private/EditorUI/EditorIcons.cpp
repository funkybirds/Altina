#include "EditorUI/EditorIcons.h"

#include "Container/String.h"

namespace AltinaEngine::Editor::UI::Private {
    namespace {
        using Core::Container::FString;
        using Core::Container::FStringView;

        [[nodiscard]] auto MatchesAny(
            FStringView lowerText, std::initializer_list<FStringView> values) -> bool {
            for (const auto value : values) {
                if (lowerText.Contains(value)) {
                    return true;
                }
            }
            return false;
        }
    } // namespace

    auto ToIconId(EEditorIconId icon) noexcept -> DebugGui::FDebugGuiIconId {
        return static_cast<DebugGui::FDebugGuiIconId>(icon);
    }

    auto GetPanelIconId(FStringView panelTitle) noexcept -> DebugGui::FDebugGuiIconId {
        if (panelTitle == FStringView(TEXT("Hierarchy"))) {
            return ToIconId(EEditorIconId::PanelHierarchy);
        }
        if (panelTitle == FStringView(TEXT("Viewport"))) {
            return ToIconId(EEditorIconId::PanelViewport);
        }
        if (panelTitle == FStringView(TEXT("Inspector"))) {
            return ToIconId(EEditorIconId::PanelInspector);
        }
        if (panelTitle == FStringView(TEXT("Asset"))) {
            return ToIconId(EEditorIconId::PanelAsset);
        }
        if (panelTitle == FStringView(TEXT("Output"))) {
            return ToIconId(EEditorIconId::PanelOutput);
        }
        return ToIconId(EEditorIconId::ComponentGeneric);
    }

    auto GetAssetIconId(bool bNavigateUp, bool bDirectory) noexcept -> DebugGui::FDebugGuiIconId {
        if (bNavigateUp) {
            return ToIconId(EEditorIconId::AssetNavigateUp);
        }
        return bDirectory ? ToIconId(EEditorIconId::AssetFolder)
                          : ToIconId(EEditorIconId::AssetFile);
    }

    auto GetComponentIconId(FStringView componentName, FStringView componentTypeName)
        -> DebugGui::FDebugGuiIconId {
        FString merged(componentTypeName);
        if (!merged.IsEmptyString()) {
            merged.Append(TEXT(" "));
        }
        merged.Append(componentName);
        const FString lower = merged.ToLowerCopy();
        const auto    view  = lower.ToView();

        if (MatchesAny(
                view, { TEXT("transform"), TEXT("position"), TEXT("rotation"), TEXT("scale") })) {
            return ToIconId(EEditorIconId::ComponentTransform);
        }
        if (MatchesAny(view, { TEXT("camera"), TEXT("lens"), TEXT("frustum") })) {
            return ToIconId(EEditorIconId::ComponentCamera);
        }
        if (MatchesAny(view, { TEXT("light"), TEXT("sun"), TEXT("shadow"), TEXT("lamp") })) {
            return ToIconId(EEditorIconId::ComponentLight);
        }
        if (MatchesAny(view,
                { TEXT("mesh"), TEXT("model"), TEXT("render"), TEXT("staticmesh"),
                    TEXT("skeletalmesh") })) {
            return ToIconId(EEditorIconId::ComponentMesh);
        }
        if (MatchesAny(view, { TEXT("script"), TEXT("code"), TEXT("lua"), TEXT("python") })) {
            return ToIconId(EEditorIconId::ComponentScript);
        }
        if (MatchesAny(view,
                { TEXT("physics"), TEXT("rigidbody"), TEXT("collider"), TEXT("collision"),
                    TEXT("joint") })) {
            return ToIconId(EEditorIconId::ComponentPhysics);
        }
        return ToIconId(EEditorIconId::ComponentGeneric);
    }
} // namespace AltinaEngine::Editor::UI::Private
