#include "EditorUI/EditorUiModule.h"

#include "DebugGui/DebugGui.h"
#include "Math/Common.h"
#include "Math/Vector.h"

namespace AltinaEngine::Editor::UI {
    namespace {
        using ::AltinaEngine::Core::Container::FStringView;
        using Core::Math::Clamp;
        using Core::Math::FVector2f;
        using DebugGui::FRect;

        constexpr f32      kMenuBarHeight   = 24.0f;
        constexpr f32      kWorkspacePad    = 4.0f;
        constexpr f32      kSplitterSize    = 4.0f;
        constexpr f32      kTabBarHeight    = 22.0f;
        constexpr f32      kPanelPadding    = 6.0f;
        constexpr f32      kMinPanelWidth   = 140.0f;
        constexpr f32      kMinCenterWidth  = 260.0f;
        constexpr f32      kMinTopHeight    = 180.0f;
        constexpr f32      kMinBottomHeight = 100.0f;
        constexpr f32      kGlyphW          = 8.0f;
        constexpr f32      kGlyphH          = 11.0f;

        [[nodiscard]] auto MakeRect(f32 x0, f32 y0, f32 x1, f32 y1) -> FRect {
            return { FVector2f(x0, y0), FVector2f(x1, y1) };
        }

        [[nodiscard]] auto IsInside(const FRect& r, const FVector2f& p) -> bool {
            return p.X() >= r.Min.X() && p.X() < r.Max.X() && p.Y() >= r.Min.Y()
                && p.Y() < r.Max.Y();
        }

        [[nodiscard]] auto TextWidth(FStringView text) -> f32 {
            return static_cast<f32>(text.Length()) * kGlyphW;
        }
    } // namespace

    void FEditorUiModule::RegisterDefaultPanels(DebugGui::IDebugGuiSystem* debugGuiSystem) {
        if (debugGuiSystem == nullptr) {
            return;
        }

        mDebugGuiSystem = debugGuiSystem;
        if (mRegistered) {
            return;
        }
        mRegistered = true;

        mPanels.Clear();
        mPanels.PushBack(
            { ::AltinaEngine::Core::Container::FString(TEXT("Hierarchy")), EDockArea::Left, true });
        mPanels.PushBack({ ::AltinaEngine::Core::Container::FString(TEXT("Viewport")),
            EDockArea::Center, true });
        mPanels.PushBack({ ::AltinaEngine::Core::Container::FString(TEXT("Inspector")),
            EDockArea::Right, true });
        mPanels.PushBack(
            { ::AltinaEngine::Core::Container::FString(TEXT("Output")), EDockArea::Bottom, true });

        debugGuiSystem->RegisterBackgroundOverlay(TEXT("Editor.UI.Root"),
            [this, debugGuiSystem](DebugGui::IDebugGui& gui) { DrawRootUi(debugGuiSystem, gui); });
    }

    auto FEditorUiModule::GetViewportRequest() const noexcept -> FEditorViewportRequest {
        return mViewportRequest;
    }

    auto FEditorUiModule::ConsumeUiCommands()
        -> ::AltinaEngine::Core::Container::TVector<EEditorUiCommand> {
        auto out = mPendingCommands;
        mPendingCommands.Clear();
        return out;
    }

    void FEditorUiModule::DrawRootUi(
        DebugGui::IDebugGuiSystem* debugGuiSystem, DebugGui::IDebugGui& gui) {
        mViewportRequest = {};

        const auto display = gui.GetDisplaySize();
        if (display.X() <= 0.0f || display.Y() <= 0.0f) {
            return;
        }

        const auto  mouse         = gui.GetMousePos();
        const bool  mousePressed  = gui.WasMousePressed();
        const bool  mouseReleased = gui.WasMouseReleased();
        const bool  mouseDown     = gui.IsMouseDown();

        const auto  colBg         = DebugGui::MakeColor32(18, 20, 24, 255);
        const auto  colMenuBg     = DebugGui::MakeColor32(28, 31, 36, 255);
        const auto  colMenuHover  = DebugGui::MakeColor32(48, 54, 63, 255);
        const auto  colBorder     = DebugGui::MakeColor32(88, 94, 108, 255);
        const auto  colPanelBg    = DebugGui::MakeColor32(32, 35, 42, 255);
        const auto  colPanelTitle = DebugGui::MakeColor32(42, 46, 56, 255);
        const auto  colText       = DebugGui::MakeColor32(224, 226, 230, 255);
        const auto  colMutedText  = DebugGui::MakeColor32(160, 168, 180, 255);
        const auto  colAccent     = DebugGui::MakeColor32(70, 125, 200, 255);
        const auto  colDropHint   = DebugGui::MakeColor32(80, 140, 220, 72);

        const FRect rootRect = MakeRect(0.0f, 0.0f, display.X(), display.Y());
        gui.DrawRectFilled(rootRect, colBg);

        const FRect menuRect = MakeRect(0.0f, 0.0f, display.X(), kMenuBarHeight);
        gui.DrawRectFilled(menuRect, colMenuBg);
        gui.DrawRect(menuRect, colBorder, 1.0f);

        auto queueCommand = [this](EEditorUiCommand cmd) { mPendingCommands.PushBack(cmd); };

        struct FMenuState {
            const TChar* Name      = TEXT("");
            i32          MenuIndex = -1;
            FRect        Rect{};
        };

        FMenuState   menus[3]{};
        const TChar* menuNames[3] = { TEXT("File"), TEXT("View"), TEXT("Play") };

        f32          menuX = 8.0f;
        for (i32 i = 0; i < 3; ++i) {
            const f32 labelW   = TextWidth(FStringView(menuNames[i]));
            const f32 itemW    = labelW + 18.0f;
            menus[i].Name      = menuNames[i];
            menus[i].MenuIndex = i;
            menus[i].Rect      = MakeRect(menuX, 2.0f, menuX + itemW, kMenuBarHeight - 2.0f);
            const bool hovered = IsInside(menus[i].Rect, mouse);
            if (hovered) {
                gui.DrawRectFilled(menus[i].Rect, colMenuHover);
            }
            gui.DrawText(FVector2f(menuX + 9.0f, 7.0f), colText, FStringView(menuNames[i]));
            if (hovered && mousePressed) {
                mOpenMenu = (mOpenMenu == i) ? -1 : i;
            }
            menuX += itemW + 4.0f;
        }

        auto drawMenuItem = [&](const FRect& itemRect, FStringView label, bool checked,
                                bool enabled, auto&& onClick) {
            const bool hovered = IsInside(itemRect, mouse);
            if (hovered) {
                gui.DrawRectFilled(itemRect, colMenuHover);
            }
            if (checked) {
                const FRect marker = MakeRect(itemRect.Min.X() + 4.0f, itemRect.Min.Y() + 4.0f,
                    itemRect.Min.X() + 10.0f, itemRect.Max.Y() - 4.0f);
                gui.DrawRectFilled(marker, colAccent);
            }
            gui.DrawText(FVector2f(itemRect.Min.X() + 14.0f, itemRect.Min.Y() + 4.0f),
                enabled ? colText : colMutedText, label);
            if (enabled && hovered && mousePressed) {
                onClick();
                mOpenMenu = -1;
            }
        };

        const bool  blockWorkspaceInput = (mOpenMenu >= 0 && mOpenMenu < 3);

        const FRect workspaceRect = MakeRect(kWorkspacePad, menuRect.Max.Y() + kWorkspacePad,
            display.X() - kWorkspacePad, display.Y() - kWorkspacePad);
        if (workspaceRect.Max.X() <= workspaceRect.Min.X()
            || workspaceRect.Max.Y() <= workspaceRect.Min.Y()) {
            return;
        }

        f32 workspaceW = workspaceRect.Max.X() - workspaceRect.Min.X();
        f32 workspaceH = workspaceRect.Max.Y() - workspaceRect.Min.Y();

        f32 leftW  = Clamp(workspaceW * mDock.LeftRatio, kMinPanelWidth,
             workspaceW - kMinCenterWidth - kMinPanelWidth - 2.0f * kSplitterSize);
        f32 rightW = Clamp(workspaceW * mDock.RightRatio, kMinPanelWidth,
            workspaceW - kMinCenterWidth - leftW - 2.0f * kSplitterSize);
        if (leftW + rightW + kMinCenterWidth + 2.0f * kSplitterSize > workspaceW) {
            rightW = workspaceW - leftW - kMinCenterWidth - 2.0f * kSplitterSize;
            if (rightW < kMinPanelWidth) {
                rightW = kMinPanelWidth;
                leftW  = workspaceW - rightW - kMinCenterWidth - 2.0f * kSplitterSize;
            }
        }

        f32   topH    = Clamp(workspaceH * (1.0f - mDock.BottomRatio), kMinTopHeight,
                 workspaceH - kMinBottomHeight - kSplitterSize);
        f32   bottomH = workspaceH - topH - kSplitterSize;

        f32   x0       = workspaceRect.Min.X();
        f32   y0       = workspaceRect.Min.Y();
        FRect leftRect = MakeRect(x0, y0, x0 + leftW, y0 + topH);
        FRect splitV1 = MakeRect(leftRect.Max.X(), y0, leftRect.Max.X() + kSplitterSize, y0 + topH);
        FRect centerRect = MakeRect(splitV1.Max.X(), y0,
            splitV1.Max.X() + (workspaceW - leftW - rightW - 2.0f * kSplitterSize), y0 + topH);
        FRect splitV2 =
            MakeRect(centerRect.Max.X(), y0, centerRect.Max.X() + kSplitterSize, y0 + topH);
        FRect rightRect  = MakeRect(splitV2.Max.X(), y0, workspaceRect.Max.X(), y0 + topH);
        FRect splitH     = MakeRect(workspaceRect.Min.X(), leftRect.Max.Y(), workspaceRect.Max.X(),
                leftRect.Max.Y() + kSplitterSize);
        FRect bottomRect = MakeRect(
            workspaceRect.Min.X(), splitH.Max.Y(), workspaceRect.Max.X(), workspaceRect.Max.Y());

        if (!blockWorkspaceInput && mousePressed) {
            if (IsInside(splitV1, mouse)) {
                mActiveSplitter = 1;
            } else if (IsInside(splitV2, mouse)) {
                mActiveSplitter = 2;
            } else if (IsInside(splitH, mouse)) {
                mActiveSplitter = 3;
            }
        }
        if (!blockWorkspaceInput && !mouseDown) {
            mActiveSplitter = 0;
        }
        if (!blockWorkspaceInput && mouseDown) {
            if (mActiveSplitter == 1) {
                const f32 relative = mouse.X() - workspaceRect.Min.X();
                leftW              = Clamp(relative, kMinPanelWidth,
                                 workspaceW - rightW - kMinCenterWidth - 2.0f * kSplitterSize);
                mDock.LeftRatio    = leftW / workspaceW;
            } else if (mActiveSplitter == 2) {
                const f32 rightBoundary = workspaceRect.Max.X() - mouse.X();
                rightW                  = Clamp(rightBoundary, kMinPanelWidth,
                                     workspaceW - leftW - kMinCenterWidth - 2.0f * kSplitterSize);
                mDock.RightRatio        = rightW / workspaceW;
            } else if (mActiveSplitter == 3) {
                const f32 relativeTop = mouse.Y() - workspaceRect.Min.Y();
                topH                  = Clamp(
                    relativeTop, kMinTopHeight, workspaceH - kMinBottomHeight - kSplitterSize);
                bottomH           = workspaceH - topH - kSplitterSize;
                mDock.BottomRatio = bottomH / workspaceH;
            }
        }

        leftRect   = MakeRect(x0, y0, x0 + leftW, y0 + topH);
        splitV1    = MakeRect(leftRect.Max.X(), y0, leftRect.Max.X() + kSplitterSize, y0 + topH);
        centerRect = MakeRect(splitV1.Max.X(), y0,
            splitV1.Max.X() + (workspaceW - leftW - rightW - 2.0f * kSplitterSize), y0 + topH);
        splitV2   = MakeRect(centerRect.Max.X(), y0, centerRect.Max.X() + kSplitterSize, y0 + topH);
        rightRect = MakeRect(splitV2.Max.X(), y0, workspaceRect.Max.X(), y0 + topH);
        splitH    = MakeRect(workspaceRect.Min.X(), leftRect.Max.Y(), workspaceRect.Max.X(),
               leftRect.Max.Y() + kSplitterSize);
        bottomRect = MakeRect(
            workspaceRect.Min.X(), splitH.Max.Y(), workspaceRect.Max.X(), workspaceRect.Max.Y());

        auto activeForArea = [&](EDockArea area) -> i32& {
            if (area == EDockArea::Left) {
                return mDock.ActiveLeft;
            }
            if (area == EDockArea::Center) {
                return mDock.ActiveCenter;
            }
            if (area == EDockArea::Right) {
                return mDock.ActiveRight;
            }
            return mDock.ActiveBottom;
        };

        struct FAreaInfo {
            EDockArea Area = EDockArea::Center;
            FRect     Rect{};
        };
        FAreaInfo areas[4] = {
            { EDockArea::Left, leftRect },
            { EDockArea::Center, centerRect },
            { EDockArea::Right, rightRect },
            { EDockArea::Bottom, bottomRect },
        };

        if (!blockWorkspaceInput && mDraggingPanel >= 0 && mouseReleased
            && mDraggingPanel < static_cast<i32>(mPanels.Size())) {
            for (const auto& area : areas) {
                if (IsInside(area.Rect, mouse)) {
                    mPanels[static_cast<usize>(mDraggingPanel)].Area = area.Area;
                    activeForArea(area.Area)                         = mDraggingPanel;
                    break;
                }
            }
            mDraggingPanel = -1;
        }

        auto drawArea = [&](const FAreaInfo& areaInfo) {
            gui.DrawRectFilled(areaInfo.Rect, colPanelBg);
            gui.DrawRect(areaInfo.Rect, colBorder, 1.0f);

            const FRect tabBar = MakeRect(areaInfo.Rect.Min.X(), areaInfo.Rect.Min.Y(),
                areaInfo.Rect.Max.X(), areaInfo.Rect.Min.Y() + kTabBarHeight);
            gui.DrawRectFilled(tabBar, colPanelTitle);
            gui.DrawRect(tabBar, colBorder, 1.0f);

            ::AltinaEngine::Core::Container::TVector<i32> panelIndices;
            for (usize i = 0; i < mPanels.Size(); ++i) {
                auto& panel = mPanels[i];
                if (panel.bVisible && panel.Area == areaInfo.Area) {
                    panelIndices.PushBack(static_cast<i32>(i));
                }
            }

            i32& activePanel = activeForArea(areaInfo.Area);
            if (panelIndices.IsEmpty()) {
                activePanel = -1;
            } else {
                bool activeFound = false;
                for (i32 idx : panelIndices) {
                    if (idx == activePanel) {
                        activeFound = true;
                        break;
                    }
                }
                if (!activeFound) {
                    activePanel = panelIndices[0];
                }
            }

            f32 tabX = tabBar.Min.X() + 4.0f;
            for (i32 panelIndex : panelIndices) {
                const auto& panel = mPanels[static_cast<usize>(panelIndex)];
                const f32   tabW  = TextWidth(panel.Name.ToView()) + 18.0f;
                FRect       tabRect =
                    MakeRect(tabX, tabBar.Min.Y() + 2.0f, tabX + tabW, tabBar.Max.Y() - 2.0f);
                const bool hovered  = IsInside(tabRect, mouse);
                const bool selected = (activePanel == panelIndex);
                gui.DrawRectFilled(
                    tabRect, selected ? colAccent : (hovered ? colMenuHover : colPanelTitle));
                gui.DrawRect(tabRect, colBorder, 1.0f);
                gui.DrawText(FVector2f(tabRect.Min.X() + 8.0f, tabRect.Min.Y() + 4.0f), colText,
                    panel.Name.ToView());

                if (!blockWorkspaceInput && hovered && mousePressed) {
                    activePanel    = panelIndex;
                    mDraggingPanel = panelIndex;
                }
                tabX += tabW + 2.0f;
            }

            const FRect contentRect =
                MakeRect(areaInfo.Rect.Min.X() + kPanelPadding, tabBar.Max.Y() + kPanelPadding,
                    areaInfo.Rect.Max.X() - kPanelPadding, areaInfo.Rect.Max.Y() - kPanelPadding);
            if (contentRect.Max.X() <= contentRect.Min.X()
                || contentRect.Max.Y() <= contentRect.Min.Y()) {
                return;
            }

            if (activePanel < 0 || activePanel >= static_cast<i32>(mPanels.Size())) {
                gui.DrawText(FVector2f(contentRect.Min.X() + 4.0f, contentRect.Min.Y() + 4.0f),
                    colMutedText, TEXT("No panel"));
                return;
            }

            const auto& panel = mPanels[static_cast<usize>(activePanel)];
            if (panel.Name.ToView() == FStringView(TEXT("Viewport"))) {
                mViewportRequest.Width = static_cast<u32>(
                    Clamp(contentRect.Max.X() - contentRect.Min.X(), 0.0f, 8192.0f));
                mViewportRequest.Height = static_cast<u32>(
                    Clamp(contentRect.Max.Y() - contentRect.Min.Y(), 0.0f, 8192.0f));
                mViewportRequest.bHasContent =
                    (mViewportRequest.Width > 0U && mViewportRequest.Height > 0U);
                mViewportRequest.bHovered = IsInside(contentRect, mouse);

                gui.DrawRectFilled(contentRect, DebugGui::MakeColor32(12, 14, 18, 255));
                gui.DrawRect(contentRect, colBorder, 1.0f);
                gui.DrawImage(
                    contentRect, kEditorViewportImageId, DebugGui::MakeColor32(255, 255, 255, 255));
                gui.DrawText(FVector2f(contentRect.Min.X() + 6.0f, contentRect.Min.Y() + 6.0f),
                    colMutedText, TEXT("Runtime View"));
                return;
            }

            if (panel.Name.ToView() == FStringView(TEXT("Hierarchy"))) {
                gui.DrawText(FVector2f(contentRect.Min.X() + 4.0f, contentRect.Min.Y() + 2.0f),
                    colText, TEXT("World"));
                gui.DrawText(FVector2f(contentRect.Min.X() + 14.0f, contentRect.Min.Y() + 18.0f),
                    colMutedText, TEXT("Editor.DefaultCamera"));
                gui.DrawText(FVector2f(contentRect.Min.X() + 14.0f, contentRect.Min.Y() + 34.0f),
                    colMutedText, TEXT("DirectionalLight"));
                return;
            }

            if (panel.Name.ToView() == FStringView(TEXT("Inspector"))) {
                gui.DrawText(FVector2f(contentRect.Min.X() + 4.0f, contentRect.Min.Y() + 2.0f),
                    colText, TEXT("Inspector"));
                gui.DrawText(FVector2f(contentRect.Min.X() + 4.0f, contentRect.Min.Y() + 20.0f),
                    colMutedText, TEXT("Select an object to edit properties."));
                return;
            }

            gui.DrawText(FVector2f(contentRect.Min.X() + 4.0f, contentRect.Min.Y() + 2.0f), colText,
                TEXT("Output"));
            gui.DrawText(FVector2f(contentRect.Min.X() + 4.0f, contentRect.Min.Y() + 20.0f),
                colMutedText, TEXT("Editor initialized."));
            gui.DrawText(FVector2f(contentRect.Min.X() + 4.0f, contentRect.Min.Y() + 36.0f),
                colMutedText, TEXT("Use File/View/Play menus above."));
        };

        drawArea(areas[0]);
        drawArea(areas[1]);
        drawArea(areas[2]);
        drawArea(areas[3]);

        gui.DrawRectFilled(splitV1,
            (mActiveSplitter == 1 || IsInside(splitV1, mouse)) ? colAccent : colPanelTitle);
        gui.DrawRectFilled(splitV2,
            (mActiveSplitter == 2 || IsInside(splitV2, mouse)) ? colAccent : colPanelTitle);
        gui.DrawRectFilled(
            splitH, (mActiveSplitter == 3 || IsInside(splitH, mouse)) ? colAccent : colPanelTitle);

        if (mDraggingPanel >= 0 && mDraggingPanel < static_cast<i32>(mPanels.Size())) {
            for (const auto& area : areas) {
                if (IsInside(area.Rect, mouse)) {
                    gui.DrawRectFilled(area.Rect, colDropHint);
                }
            }
            const auto& panel   = mPanels[static_cast<usize>(mDraggingPanel)];
            const FRect dragTag = MakeRect(mouse.X() + 12.0f, mouse.Y() + 12.0f,
                mouse.X() + 12.0f + TextWidth(panel.Name.ToView()) + 14.0f, mouse.Y() + 30.0f);
            gui.DrawRectFilled(dragTag, colMenuHover);
            gui.DrawRect(dragTag, colBorder, 1.0f);
            gui.DrawText(FVector2f(dragTag.Min.X() + 7.0f, dragTag.Min.Y() + 4.0f), colText,
                panel.Name.ToView());
        }

        if (mOpenMenu >= 0 && mOpenMenu < 3) {
            const FRect anchor    = menus[mOpenMenu].Rect;
            const f32   itemH     = 20.0f;
            const f32   menuW     = (mOpenMenu == 1) ? 220.0f : 180.0f;
            const usize itemCount = (mOpenMenu == 1) ? 7U : 5U;
            const FRect dropRect =
                MakeRect(anchor.Min.X(), menuRect.Max.Y(), anchor.Min.X() + menuW,
                    menuRect.Max.Y() + static_cast<f32>(itemCount) * itemH + 4.0f);
            gui.DrawRectFilled(dropRect, colMenuBg);
            gui.DrawRect(dropRect, colBorder, 1.0f);

            auto itemRect = [&](usize idx) -> FRect {
                const f32 y0 = dropRect.Min.Y() + 2.0f + static_cast<f32>(idx) * itemH;
                return MakeRect(dropRect.Min.X() + 2.0f, y0, dropRect.Max.X() - 2.0f, y0 + itemH);
            };

            if (mOpenMenu == 0) {
                drawMenuItem(itemRect(0), TEXT("New Project"), false, false, [] {});
                drawMenuItem(itemRect(1), TEXT("Open Project"), false, false, [] {});
                drawMenuItem(itemRect(2), TEXT("Save"), false, false, [] {});
                drawMenuItem(itemRect(3), TEXT("Save All"), false, false, [] {});
                drawMenuItem(itemRect(4), TEXT("Exit"), false, true,
                    [&]() { queueCommand(EEditorUiCommand::Exit); });
            } else if (mOpenMenu == 1) {
                auto panelChecked = [&](FStringView panelName) -> bool {
                    for (auto& panel : mPanels) {
                        if (panel.Name.ToView() == panelName) {
                            return panel.bVisible;
                        }
                    }
                    return false;
                };
                auto togglePanel = [&](FStringView panelName) {
                    for (auto& panel : mPanels) {
                        if (panel.Name.ToView() == panelName) {
                            panel.bVisible = !panel.bVisible;
                        }
                    }
                };

                drawMenuItem(itemRect(0), TEXT("Hierarchy"), panelChecked(TEXT("Hierarchy")), true,
                    [&]() { togglePanel(TEXT("Hierarchy")); });
                drawMenuItem(itemRect(1), TEXT("Viewport"), panelChecked(TEXT("Viewport")), true,
                    [&]() { togglePanel(TEXT("Viewport")); });
                drawMenuItem(itemRect(2), TEXT("Inspector"), panelChecked(TEXT("Inspector")), true,
                    [&]() { togglePanel(TEXT("Inspector")); });
                drawMenuItem(itemRect(3), TEXT("Output"), panelChecked(TEXT("Output")), true,
                    [&]() { togglePanel(TEXT("Output")); });

                const bool statsShown =
                    (debugGuiSystem != nullptr) ? debugGuiSystem->IsStatsShown() : false;
                const bool consoleShown =
                    (debugGuiSystem != nullptr) ? debugGuiSystem->IsConsoleShown() : false;
                const bool cvarsShown =
                    (debugGuiSystem != nullptr) ? debugGuiSystem->IsCVarsShown() : false;
                drawMenuItem(itemRect(4), TEXT("Debug Stats"), statsShown,
                    debugGuiSystem != nullptr,
                    [&]() { debugGuiSystem->SetShowStats(!statsShown); });
                drawMenuItem(itemRect(5), TEXT("Debug Console"), consoleShown,
                    debugGuiSystem != nullptr,
                    [&]() { debugGuiSystem->SetShowConsole(!consoleShown); });
                drawMenuItem(itemRect(6), TEXT("Debug CVars"), cvarsShown,
                    debugGuiSystem != nullptr,
                    [&]() { debugGuiSystem->SetShowCVars(!cvarsShown); });
            } else {
                drawMenuItem(itemRect(0), TEXT("Play"), false, true,
                    [&]() { queueCommand(EEditorUiCommand::Play); });
                drawMenuItem(itemRect(1), TEXT("Pause"), false, true,
                    [&]() { queueCommand(EEditorUiCommand::Pause); });
                drawMenuItem(itemRect(2), TEXT("Step"), false, true,
                    [&]() { queueCommand(EEditorUiCommand::Step); });
                drawMenuItem(itemRect(3), TEXT("Stop"), false, true,
                    [&]() { queueCommand(EEditorUiCommand::Stop); });
                drawMenuItem(itemRect(4), TEXT("Simulate"), false, false, [] {});
            }

            if (mousePressed && !IsInside(dropRect, mouse) && !IsInside(anchor, mouse)) {
                mOpenMenu = -1;
            }
        } else if (mousePressed && mouse.Y() > menuRect.Max.Y()) {
            mOpenMenu = -1;
        }

        if (!blockWorkspaceInput && mousePressed) {
            mFocusedViewport = mViewportRequest.bHovered;
        }
        mViewportRequest.bFocused = mFocusedViewport;
    }
} // namespace AltinaEngine::Editor::UI
