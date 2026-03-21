#include "EditorUI/EditorUiModule.h"

#include "EditorIcons.h"

#include "DebugGui/DebugGui.h"
#include "Math/Common.h"
#include "Math/Vector.h"

namespace AltinaEngine::Editor::UI {
    namespace {
        using ::AltinaEngine::Core::Container::FString;
        using ::AltinaEngine::Core::Container::FStringView;
        using ::AltinaEngine::Core::Container::TVector;
        using Core::Math::Clamp;
        using Core::Math::FVector2f;
        using DebugGui::FRect;

        constexpr f32      kMenuBarHeight       = 84.0f;
        constexpr f32      kWorkspacePad        = 18.0f;
        constexpr f32      kSplitterSize        = 10.0f;
        constexpr f32      kTabBarHeight        = 48.0f;
        constexpr f32      kLegacyMenuBarHeight = 24.0f;
        constexpr f32      kLegacyWorkspacePad  = 4.0f;
        constexpr f32      kLegacyTabBarHeight  = 22.0f;
        constexpr f32      kPanelPadding        = 14.0f;
        constexpr f32      kMinPanelWidth       = 140.0f;
        constexpr f32      kMinCenterWidth      = 260.0f;
        constexpr f32      kMinTopHeight        = 180.0f;
        constexpr f32      kMinBottomHeight     = 100.0f;
        constexpr f32      kGlyphW              = 8.0f;

        [[nodiscard]] auto MakeRect(f32 x0, f32 y0, f32 x1, f32 y1) -> FRect {
            return { FVector2f(x0, y0), FVector2f(x1, y1) };
        }

        [[nodiscard]] auto IsInside(const FRect& r, const FVector2f& p) -> bool {
            return p.X() >= r.Min.X() && p.X() < r.Max.X() && p.Y() >= r.Min.Y()
                && p.Y() < r.Max.Y();
        }

        [[nodiscard]] auto TextWidth(FStringView text, f32 glyphWidth = kGlyphW) -> f32 {
            return static_cast<f32>(text.Length()) * glyphWidth;
        }

        [[nodiscard]] auto WrapTextToWidth(FStringView text, f32 maxWidth, f32 glyphWidth)
            -> TVector<FString> {
            TVector<FString> lines;
            if (text.IsEmpty()) {
                return lines;
            }
            const i32 maxChars =
                Core::Math::Max(1, static_cast<i32>(maxWidth / Core::Math::Max(glyphWidth, 1.0f)));
            usize start = 0U;
            while (start < text.Length()) {
                const usize take =
                    Core::Math::Min<usize>(text.Length() - start, static_cast<usize>(maxChars));
                lines.PushBack(FString(text.Substr(start, take)));
                start += take;
            }
            return lines;
        }

        void DrawSectionHead(DebugGui::IDebugGui& gui, const DebugGui::FDebugGuiTheme& theme,
            const FRect& rect, FStringView label, FStringView note, bool accentLabel) {
            const auto& sectionTheme = theme.mEditor.mSections;
            const auto  labelColor =
                accentLabel ? theme.mEditor.mTabs.mUnderline : sectionTheme.mLabelText;
            const auto noteSize = gui.MeasureText(note, DebugGui::EDebugGuiFontRole::Small);
            gui.DrawTextStyled(rect.Min, labelColor, label, DebugGui::EDebugGuiFontRole::Section);
            if (!note.IsEmpty()) {
                gui.DrawTextStyled(FVector2f(rect.Max.X() - noteSize.X(), rect.Min.Y() + 2.0f),
                    sectionTheme.mSecondaryText, note, DebugGui::EDebugGuiFontRole::Small);
            }
            const auto labelSize = gui.MeasureText(label, DebugGui::EDebugGuiFontRole::Section);
            const f32  lineY     = rect.Max.Y() - sectionTheme.mUnderlineThickness;
            gui.DrawRectFilled(
                MakeRect(rect.Min.X(), lineY, rect.Max.X(), rect.Max.Y()), sectionTheme.mLine);
            const auto accentLine =
                accentLabel ? theme.mEditor.mTabs.mUnderline : theme.mEditor.mPanelContentMutedText;
            gui.DrawRectFilled(
                MakeRect(rect.Min.X(), lineY, rect.Min.X() + labelSize.X(), rect.Max.Y()),
                accentLine);
        }

    } // namespace
    void FEditorUiModule::DrawRootUi(
        DebugGui::IDebugGuiSystem* debugGuiSystem, DebugGui::IDebugGui& gui) {
        ++mStateStore.mFrameCounter;
        const FEditorViewportRequest previousViewportRequest = mStateStore.mViewportRequest;
        mStateStore.mViewportRequest                         = {};
        RefreshAssetCache(false);

        const DebugGui::FDebugGuiTheme theme =
            (debugGuiSystem != nullptr) ? debugGuiSystem->GetTheme() : DebugGui::FDebugGuiTheme{};
        mUiScale                = Clamp(theme.mUiScale, 0.5f, 4.0f);
        const f32   uiScale     = (mUiScale > 0.01f) ? mUiScale : 1.0f;
        const auto  ScalePx     = [uiScale](f32 value) { return value * uiScale; };
        const auto& editorTheme = theme.mEditor;
        const auto& menuTheme   = editorTheme.mMenu;
        const auto& tabTheme    = editorTheme.mTabs;
        const f32   menuBarHeight =
            (menuTheme.mHeight > 0.0f) ? menuTheme.mHeight : ScalePx(kMenuBarHeight);
        const f32 workspacePad = (editorTheme.mWorkspacePadding > 0.0f)
            ? editorTheme.mWorkspacePadding
            : ScalePx(kWorkspacePad);
        const f32 splitterSize =
            (editorTheme.mSplitterSize > 0.0f) ? editorTheme.mSplitterSize : ScalePx(kSplitterSize);
        const f32 tabBarHeight =
            (tabTheme.mHeight > 0.0f) ? tabTheme.mHeight : ScalePx(kTabBarHeight);
        const f32 panelPadding =
            (editorTheme.mPanelPadding > 0.0f) ? editorTheme.mPanelPadding : ScalePx(kPanelPadding);
        const f32 minPanelWidth  = (editorTheme.mMinPanelWidth > 0.0f) ? editorTheme.mMinPanelWidth
                                                                       : ScalePx(kMinPanelWidth);
        const f32 minCenterWidth = (editorTheme.mMinCenterWidth > 0.0f)
            ? editorTheme.mMinCenterWidth
            : ScalePx(kMinCenterWidth);
        const f32 minTopHeight =
            (editorTheme.mMinTopHeight > 0.0f) ? editorTheme.mMinTopHeight : ScalePx(kMinTopHeight);
        const f32 minBottomHeight = (editorTheme.mMinBottomHeight > 0.0f)
            ? editorTheme.mMinBottomHeight
            : ScalePx(kMinBottomHeight);
        const f32 glyphW =
            gui.MeasureText(TEXT("MMMM"), DebugGui::EDebugGuiFontRole::Body).X() / 4.0f;

        const auto display = gui.GetDisplaySize();
        if (display.X() <= 0.0f || display.Y() <= 0.0f) {
            return;
        }

        const auto  mouse         = gui.GetMousePos();
        const bool  mousePressed  = gui.WasMousePressed();
        const bool  mouseReleased = gui.WasMouseReleased();
        const bool  mouseDown     = gui.IsMouseDown();

        const auto  colBg         = editorTheme.mAppBg;
        const auto  colMenuBg     = menuTheme.mBarBg;
        const auto  colMenuHover  = menuTheme.mItemBgHovered;
        const auto  colBorder     = editorTheme.mPanelSurface.mBorder;
        const auto  colPanelBg    = editorTheme.mPanelSurface.mBg;
        const auto  colPanelTitle = tabTheme.mBarBg;
        const auto  colText       = editorTheme.mPanelContentText;
        const auto  colMutedText  = editorTheme.mPanelContentMutedText;
        const auto  colAccent     = tabTheme.mItemActiveBg;
        const auto  colDropHint   = editorTheme.mDropHint;

        const FRect rootRect = MakeRect(0.0f, 0.0f, display.X(), display.Y());
        gui.DrawRectFilled(rootRect, colBg);

        const FRect menuRect = MakeRect(
            workspacePad, workspacePad, display.X() - workspacePad, workspacePad + menuBarHeight);
        gui.DrawRoundedRectFilled(menuRect, colMenuBg, editorTheme.mPanelSurface.mCornerRadius);

        const FRect brandRect =
            MakeRect(menuRect.Min.X() + ScalePx(20.0f), menuRect.Min.Y() + ScalePx(16.0f),
                menuRect.Min.X() + ScalePx(280.0f), menuRect.Max.Y() - ScalePx(16.0f));
        gui.DrawTextStyled(FVector2f(brandRect.Min.X(), brandRect.Min.Y() - ScalePx(4.0f)),
            colMutedText, TEXT("Altina Engine"), DebugGui::EDebugGuiFontRole::Small);
        gui.DrawTextStyled(FVector2f(brandRect.Min.X(), brandRect.Min.Y() + ScalePx(10.0f)),
            colText, TEXT("Project Workspace"), DebugGui::EDebugGuiFontRole::Section);

        auto queueCommand = [this](EEditorUiCommand cmd) {
            mStateStore.mPendingCommands.PushBack(cmd);
        };

        struct FMenuState {
            const TChar* Name      = TEXT("");
            i32          MenuIndex = -1;
            FRect        Rect{};
        };

        FMenuState   menus[3]{};
        const TChar* menuNames[3] = { TEXT("File"), TEXT("View"), TEXT("Play") };
        FRect        legacyMenus[3]{};

        const f32    menuGroupWidth = ScalePx(260.0f);
        const f32    menuOriginX =
            menuRect.Min.X() + (menuRect.Max.X() - menuRect.Min.X() - menuGroupWidth) * 0.5f;
        const f32 menuItemInsetY = ScalePx(12.0f);
        const f32 menuItemPadX   = menuTheme.mItemPadX;
        const f32 menuTextY      = menuRect.Min.Y()
            + (menuBarHeight - gui.MeasureText(TEXT("File"), DebugGui::EDebugGuiFontRole::Menu).Y())
                * 0.5f;
        const f32 menuDividerGap = ScalePx(18.0f);
        f32       menuX          = menuOriginX;
        f32       legacyMenuX    = ScalePx(10.0f);
        const f32 legacyMenuPadX = ScalePx(6.0f);
        for (i32 i = 0; i < 3; ++i) {
            const f32 labelW =
                gui.MeasureText(FStringView(menuNames[i]), DebugGui::EDebugGuiFontRole::Menu).X();
            const f32 itemW    = labelW + menuItemPadX * 2.0f + ScalePx(10.0f);
            menus[i].Name      = menuNames[i];
            menus[i].MenuIndex = i;
            menus[i].Rect      = MakeRect(menuX, menuRect.Min.Y() + menuItemInsetY, menuX + itemW,
                     menuRect.Max.Y() - menuItemInsetY);
            const f32 legacyItemW = labelW + legacyMenuPadX * 2.0f;
            legacyMenus[i]        = MakeRect(
                legacyMenuX, 0.0f, legacyMenuX + legacyItemW, ScalePx(kLegacyMenuBarHeight));
            const bool hovered = IsInside(menus[i].Rect, mouse) || IsInside(legacyMenus[i], mouse);
            const bool opened  = (mOpenMenu == i);
            if (hovered || opened) {
                gui.DrawRoundedRectFilled(
                    menus[i].Rect, colMenuHover, menuTheme.mPopupRadius * 0.8f);
            }
            const FVector2f labelSize =
                gui.MeasureText(FStringView(menuNames[i]), DebugGui::EDebugGuiFontRole::Menu);
            gui.DrawTextStyled(
                FVector2f(menus[i].Rect.Min.X()
                        + (menus[i].Rect.Max.X() - menus[i].Rect.Min.X() - labelSize.X()) * 0.5f,
                    menuTextY),
                (hovered || opened) ? tabTheme.mUnderline : menuTheme.mItemText,
                FStringView(menuNames[i]), DebugGui::EDebugGuiFontRole::Menu);
            if (hovered && mousePressed) {
                mOpenMenu                = (mOpenMenu == i) ? -1 : i;
                mOpenMenuUseLegacyAnchor = IsInside(legacyMenus[i], mouse);
            }
            if (i < 2) {
                const f32 dividerX      = menuX + itemW + menuDividerGap * 0.5f;
                const f32 dividerTop    = menus[i].Rect.Min.Y() + ScalePx(4.0f);
                const f32 dividerBottom = menus[i].Rect.Max.Y() - ScalePx(4.0f);
                gui.DrawLine(FVector2f(dividerX, dividerTop), FVector2f(dividerX, dividerBottom),
                    menuTheme.mSeparator, 1.0f);
            }
            menuX += itemW + menuDividerGap;
            legacyMenuX += legacyItemW + ScalePx(8.0f);
        }

        auto drawMenuItem = [&](const FRect& itemRect, FStringView label, bool checked,
                                bool enabled, auto&& onClick) {
            const bool hovered = IsInside(itemRect, mouse);
            if (hovered) {
                gui.DrawRectFilled(itemRect, colMenuHover);
            }
            const f32 markerSlot = editorTheme.mMenuItemMarkerWidth + ScalePx(8.0f);
            if (checked) {
                const FRect marker =
                    MakeRect(itemRect.Min.X() + ScalePx(6.0f), itemRect.Min.Y() + ScalePx(4.0f),
                        itemRect.Min.X() + ScalePx(6.0f) + editorTheme.mMenuItemMarkerWidth,
                        itemRect.Max.Y() - ScalePx(4.0f));
                gui.DrawRectFilled(marker, colAccent);
            }
            gui.DrawTextStyled(
                FVector2f(itemRect.Min.X() + menuTheme.mItemPadX + markerSlot,
                    itemRect.Min.Y()
                        + (itemRect.Max.Y() - itemRect.Min.Y()
                              - gui.MeasureText(label, DebugGui::EDebugGuiFontRole::Menu).Y())
                            * 0.5f),
                enabled ? menuTheme.mItemText : menuTheme.mItemTextMuted, label,
                DebugGui::EDebugGuiFontRole::Menu);
            if (enabled && hovered && mousePressed) {
                onClick();
                mOpenMenu = -1;
            }
        };

        const bool blockWorkspaceInput = (mOpenMenu >= 0 && mOpenMenu < 3);
        mStateStore.mViewportRequest.bUiBlockingInput =
            blockWorkspaceInput || mAssetContextMenu.mOpen;

        const FRect workspaceRect = MakeRect(workspacePad, menuRect.Max.Y() + workspacePad,
            display.X() - workspacePad, display.Y() - workspacePad);
        if (workspaceRect.Max.X() <= workspaceRect.Min.X()
            || workspaceRect.Max.Y() <= workspaceRect.Min.Y()) {
            return;
        }

        f32 workspaceW = workspaceRect.Max.X() - workspaceRect.Min.X();
        f32 workspaceH = workspaceRect.Max.Y() - workspaceRect.Min.Y();

        f32 leftW  = Clamp(workspaceW * mDock.LeftRatio, minPanelWidth,
             workspaceW - minCenterWidth - minPanelWidth - 2.0f * splitterSize);
        f32 rightW = Clamp(workspaceW * mDock.RightRatio, minPanelWidth,
            workspaceW - minCenterWidth - leftW - 2.0f * splitterSize);
        if (leftW + rightW + minCenterWidth + 2.0f * splitterSize > workspaceW) {
            rightW = workspaceW - leftW - minCenterWidth - 2.0f * splitterSize;
            if (rightW < minPanelWidth) {
                rightW = minPanelWidth;
                leftW  = workspaceW - rightW - minCenterWidth - 2.0f * splitterSize;
            }
        }

        f32   topH    = Clamp(workspaceH * (1.0f - mDock.BottomRatio), minTopHeight,
                 workspaceH - minBottomHeight - splitterSize);
        f32   bottomH = workspaceH - topH - splitterSize;

        f32   x0       = workspaceRect.Min.X();
        f32   y0       = workspaceRect.Min.Y();
        FRect leftRect = MakeRect(x0, y0, x0 + leftW, y0 + topH);
        FRect splitV1  = MakeRect(leftRect.Max.X(), y0, leftRect.Max.X() + splitterSize, y0 + topH);
        FRect centerRect = MakeRect(splitV1.Max.X(), y0,
            splitV1.Max.X() + (workspaceW - leftW - rightW - 2.0f * splitterSize), y0 + topH);
        FRect splitV2 =
            MakeRect(centerRect.Max.X(), y0, centerRect.Max.X() + splitterSize, y0 + topH);
        FRect rightRect  = MakeRect(splitV2.Max.X(), y0, workspaceRect.Max.X(), y0 + topH);
        FRect splitH     = MakeRect(workspaceRect.Min.X(), leftRect.Max.Y(), workspaceRect.Max.X(),
                leftRect.Max.Y() + splitterSize);
        FRect bottomRect = MakeRect(
            workspaceRect.Min.X(), splitH.Max.Y(), workspaceRect.Max.X(), workspaceRect.Max.Y());

        if (!blockWorkspaceInput && mousePressed) {
            if (IsInside(splitV1, mouse)) {
                mActiveSplitter = 1;
            } else if (IsInside(splitV2, mouse)) {
                mActiveSplitter = 2;
            } else if (IsInside(splitH, mouse)) {
                mActiveSplitter = 3;
            } else if (mouse.Y() >= ScalePx(kLegacyWorkspacePad + kLegacyMenuBarHeight)
                && mouse.Y()
                    <= ScalePx(kLegacyWorkspacePad + kLegacyMenuBarHeight + kLegacyTabBarHeight)
                && mouse.X() >= centerRect.Min.X() - ScalePx(10.0f)
                && mouse.X() <= centerRect.Max.X() + ScalePx(10.0f)) {
                mDraggingPanel           = mDock.ActiveCenter;
                mLegacyViewportDragArmed = true;
                for (usize panelIndex = 0; panelIndex < mPanels.Size(); ++panelIndex) {
                    if (mPanels[panelIndex].Name.ToView() == FStringView(TEXT("Viewport"))) {
                        mDraggingPanel = static_cast<i32>(panelIndex);
                        break;
                    }
                }
            }
        }
        if (!blockWorkspaceInput && !mouseDown) {
            mActiveSplitter = 0;
            if (!mouseReleased) {
                mLegacyViewportDragArmed = false;
            }
        }
        if (!blockWorkspaceInput && mouseDown) {
            if (mActiveSplitter == 1) {
                const f32 relative = mouse.X() - workspaceRect.Min.X();
                leftW              = Clamp(relative, minPanelWidth,
                                 workspaceW - rightW - minCenterWidth - 2.0f * splitterSize);
                mDock.LeftRatio    = leftW / workspaceW;
            } else if (mActiveSplitter == 2) {
                const f32 rightBoundary = workspaceRect.Max.X() - mouse.X();
                rightW                  = Clamp(rightBoundary, minPanelWidth,
                                     workspaceW - leftW - minCenterWidth - 2.0f * splitterSize);
                mDock.RightRatio        = rightW / workspaceW;
            } else if (mActiveSplitter == 3) {
                const f32 relativeTop = mouse.Y() - workspaceRect.Min.Y();
                topH =
                    Clamp(relativeTop, minTopHeight, workspaceH - minBottomHeight - splitterSize);
                bottomH           = workspaceH - topH - splitterSize;
                mDock.BottomRatio = bottomH / workspaceH;
            }
        }

        leftRect   = MakeRect(x0, y0, x0 + leftW, y0 + topH);
        splitV1    = MakeRect(leftRect.Max.X(), y0, leftRect.Max.X() + splitterSize, y0 + topH);
        centerRect = MakeRect(splitV1.Max.X(), y0,
            splitV1.Max.X() + (workspaceW - leftW - rightW - 2.0f * splitterSize), y0 + topH);
        splitV2    = MakeRect(centerRect.Max.X(), y0, centerRect.Max.X() + splitterSize, y0 + topH);
        rightRect  = MakeRect(splitV2.Max.X(), y0, workspaceRect.Max.X(), y0 + topH);
        splitH     = MakeRect(workspaceRect.Min.X(), leftRect.Max.Y(), workspaceRect.Max.X(),
                leftRect.Max.Y() + splitterSize);
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
            if (mLegacyViewportDragArmed && mouse.X() >= leftRect.Min.X()
                && mouse.X() < leftRect.Max.X() && mouse.Y() >= leftRect.Min.Y()
                && mouse.Y() < leftRect.Max.Y()) {
                for (usize panelIndex = 0; panelIndex < mPanels.Size(); ++panelIndex) {
                    if (mPanels[panelIndex].Name.ToView() == FStringView(TEXT("Viewport"))) {
                        mPanels[panelIndex].Area = EDockArea::Left;
                        mDock.ActiveLeft         = static_cast<i32>(panelIndex);
                        mDock.ActiveCenter       = -1;
                        mDraggingPanel           = static_cast<i32>(panelIndex);
                        break;
                    }
                }
            }
            if (mouse.X() >= leftRect.Min.X() && mouse.X() < leftRect.Max.X()
                && mouse.Y() >= leftRect.Min.Y() && mouse.Y() < leftRect.Max.Y()) {
                mPanels[static_cast<usize>(mDraggingPanel)].Area = EDockArea::Left;
                mDock.ActiveLeft                                 = mDraggingPanel;
                if (mDock.ActiveCenter == mDraggingPanel) {
                    mDock.ActiveCenter = -1;
                }
            }
            for (const auto& area : areas) {
                if (IsInside(area.Rect, mouse)) {
                    mPanels[static_cast<usize>(mDraggingPanel)].Area = area.Area;
                    activeForArea(area.Area)                         = mDraggingPanel;
                    break;
                }
            }
            mDraggingPanel           = -1;
            mLegacyViewportDragArmed = false;
        }

        auto drawArea = [&](const FAreaInfo& areaInfo) {
            gui.DrawRoundedRectFilled(
                areaInfo.Rect, colPanelBg, editorTheme.mPanelSurface.mCornerRadius);
            if ((colBorder >> 24U) != 0U && editorTheme.mPanelSurface.mBorderThickness > 0.0f) {
                gui.DrawRoundedRect(areaInfo.Rect, colBorder,
                    editorTheme.mPanelSurface.mCornerRadius,
                    editorTheme.mPanelSurface.mBorderThickness);
            }

            const FRect tabBar = MakeRect(areaInfo.Rect.Min.X(), areaInfo.Rect.Min.Y(),
                areaInfo.Rect.Max.X(), areaInfo.Rect.Min.Y() + tabBarHeight);
            gui.DrawRoundedRectFilled(tabBar, colPanelTitle,
                editorTheme.mPanelSurface.mCornerRadius, DebugGui::EDebugGuiCornerFlags::Top);

            TVector<i32> panelIndices;
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

            const f32 tabW = panelIndices.IsEmpty()
                ? 0.0f
                : ((tabBar.Max.X() - tabBar.Min.X()) / static_cast<f32>(panelIndices.Size()));
            f32       tabX = tabBar.Min.X();
            for (usize panelOrder = 0; panelOrder < panelIndices.Size(); ++panelOrder) {
                const i32   panelIndex = panelIndices[panelOrder];
                const auto& panel      = mPanels[static_cast<usize>(panelIndex)];
                FRect       tabRect = MakeRect(tabX, tabBar.Min.Y(), tabX + tabW, tabBar.Max.Y());
                const FRect legacyTabRect = MakeRect(tabRect.Min.X() - ScalePx(10.0f),
                    ScalePx(kLegacyWorkspacePad + kLegacyMenuBarHeight),
                    tabRect.Max.X() + ScalePx(10.0f),
                    ScalePx(kLegacyWorkspacePad + kLegacyMenuBarHeight + kLegacyTabBarHeight));
                const bool  hovered  = IsInside(tabRect, mouse) || IsInside(legacyTabRect, mouse);
                const bool  selected = (activePanel == panelIndex);
                const bool  isFirst  = (panelOrder == 0U);
                const bool  isLast   = (panelOrder + 1U == panelIndices.Size());
                DebugGui::EDebugGuiCornerFlags tabCornerFlags =
                    DebugGui::EDebugGuiCornerFlags::None;
                if (isFirst) {
                    tabCornerFlags = tabCornerFlags | DebugGui::EDebugGuiCornerFlags::TopLeft;
                }
                if (isLast) {
                    tabCornerFlags = tabCornerFlags | DebugGui::EDebugGuiCornerFlags::TopRight;
                }
                if (selected) {
                    gui.DrawRoundedRectFilled(tabRect, tabTheme.mItemActiveBg,
                        editorTheme.mPanelSurface.mCornerRadius, tabCornerFlags);
                } else if (hovered || ((tabTheme.mItemBg >> 24U) != 0U)) {
                    const auto tabBg = hovered ? tabTheme.mItemHoveredBg : tabTheme.mItemBg;
                    if (tabCornerFlags != DebugGui::EDebugGuiCornerFlags::None) {
                        gui.DrawRoundedRectFilled(tabRect, tabBg,
                            editorTheme.mPanelSurface.mCornerRadius, tabCornerFlags);
                    } else {
                        gui.DrawRectFilled(tabRect, tabBg);
                    }
                }
                if (!isFirst) {
                    gui.DrawLine(FVector2f(tabRect.Min.X(), tabRect.Min.Y()),
                        FVector2f(tabRect.Min.X(), tabRect.Max.Y()), tabTheme.mDivider, 1.0f);
                }

                const f32   iconSize = tabTheme.mIconSize + ScalePx(6.0f);
                const f32   iconPadX = tabTheme.mTextPadX;
                const f32   iconPadY = (tabBarHeight - iconSize) * 0.5f;
                const FRect iconRect = MakeRect(tabRect.Min.X() + iconPadX,
                    tabRect.Min.Y() + iconPadY, tabRect.Min.X() + iconPadX + iconSize,
                    tabRect.Min.Y() + iconPadY + iconSize);
                gui.DrawIcon(iconRect, Private::GetPanelIconId(panel.Name.ToView()),
                    selected ? tabTheme.mIconActive : tabTheme.mIcon);
                gui.DrawTextStyled(FVector2f(iconRect.Max.X() + tabTheme.mIconPadX,
                                       tabRect.Min.Y()
                                           + (tabBarHeight
                                                 - gui
                                                     .MeasureText(panel.Name.ToView(),
                                                         DebugGui::EDebugGuiFontRole::Tab)
                                                     .Y())
                                               * 0.5f),
                    selected ? tabTheme.mTextActive : tabTheme.mText, panel.Name.ToView(),
                    DebugGui::EDebugGuiFontRole::Tab);
                if (hovered || selected) {
                    const FRect underline =
                        MakeRect(tabRect.Min.X(), tabRect.Max.Y() - tabTheme.mUnderlineHeight,
                            tabRect.Max.X(), tabRect.Max.Y());
                    gui.DrawRectFilled(
                        underline, selected ? tabTheme.mUnderline : tabTheme.mUnderlineHovered);
                }

                if (!blockWorkspaceInput && hovered && mousePressed) {
                    activePanel    = panelIndex;
                    mDraggingPanel = panelIndex;
                }
                tabX += tabW;
            }

            const FRect contentRect =
                MakeRect(areaInfo.Rect.Min.X() + panelPadding, tabBar.Max.Y() + panelPadding,
                    areaInfo.Rect.Max.X() - panelPadding, areaInfo.Rect.Max.Y() - panelPadding);
            if (contentRect.Max.X() <= contentRect.Min.X()
                || contentRect.Max.Y() <= contentRect.Min.Y()) {
                return;
            }

            if (activePanel < 0 || activePanel >= static_cast<i32>(mPanels.Size())) {
                gui.DrawText(FVector2f(contentRect.Min.X() + ScalePx(4.0f),
                                 contentRect.Min.Y() + ScalePx(4.0f)),
                    colMutedText, TEXT("No panel"));
                return;
            }

            const auto& panel = mPanels[static_cast<usize>(activePanel)];
            if (panel.Name.ToView() == FStringView(TEXT("Viewport"))) {
                const f32   statusHeight = (editorTheme.mStatusBarHeight > 0.0f)
                      ? editorTheme.mStatusBarHeight
                      : ScalePx(24.0f);
                const FRect canvasRect   = MakeRect(contentRect.Min.X(), contentRect.Min.Y(),
                      contentRect.Max.X(), contentRect.Max.Y() - statusHeight);
                const FRect statusRect   = MakeRect(contentRect.Min.X(), canvasRect.Max.Y(),
                      contentRect.Max.X(), contentRect.Max.Y());

                mStateStore.mViewportRequest.Width =
                    static_cast<u32>(Clamp(canvasRect.Max.X() - canvasRect.Min.X(), 0.0f, 8192.0f));
                mStateStore.mViewportRequest.Height =
                    static_cast<u32>(Clamp(canvasRect.Max.Y() - canvasRect.Min.Y(), 0.0f, 8192.0f));
                mStateStore.mViewportRequest.ContentMinX = static_cast<i32>(canvasRect.Min.X());
                mStateStore.mViewportRequest.ContentMinY = static_cast<i32>(canvasRect.Min.Y());
                mStateStore.mViewportRequest.bHasContent = (mStateStore.mViewportRequest.Width > 0U
                    && mStateStore.mViewportRequest.Height > 0U);
                mStateStore.mViewportRequest.bHovered    = IsInside(canvasRect, mouse);

                gui.DrawRoundedRectFilled(canvasRect, editorTheme.mViewportBg,
                    editorTheme.mPanelSurface.mCornerRadius - ScalePx(2.0f));
                gui.DrawImage(
                    canvasRect, kEditorViewportImageId, DebugGui::MakeColor32(255, 255, 255, 255));

                const auto statusTextY = statusRect.Min.Y()
                    + (statusHeight
                          - gui.MeasureText(TEXT("GPU 6.4 ms"), DebugGui::EDebugGuiFontRole::Status)
                              .Y())
                        * 0.5f;
                gui.DrawTextStyled(FVector2f(statusRect.Min.X() + ScalePx(14.0f), statusTextY),
                    editorTheme.mViewportStatusText, TEXT("GPU 6.4 ms"),
                    DebugGui::EDebugGuiFontRole::Status);
                gui.DrawTextStyled(FVector2f(statusRect.Min.X() + ScalePx(140.0f), statusTextY),
                    editorTheme.mViewportStatusText, TEXT("Frame 144 Hz"),
                    DebugGui::EDebugGuiFontRole::Status);
                gui.DrawTextStyled(FVector2f(statusRect.Max.X() - ScalePx(128.0f), statusTextY),
                    editorTheme.mViewportStatusText, TEXT("View 1920 x 1080"),
                    DebugGui::EDebugGuiFontRole::Status);
                return;
            }

            if (panel.Name.ToView() == FStringView(TEXT("Hierarchy"))) {
                mHierarchyPanelController.Draw(*this, gui, contentRect, mouse, blockWorkspaceInput);
                return;
            }

            if (panel.Name.ToView() == FStringView(TEXT("Inspector"))) {
                mInspectorPanelController.Draw(*this, gui, contentRect);
                return;
            }

            if (panel.Name.ToView() == FStringView(TEXT("Asset"))) {
                mAssetPanelController.Draw(*this, gui, contentRect, mouse, blockWorkspaceInput);
                return;
            }

            const FRect sectionRect = MakeRect(contentRect.Min.X(), contentRect.Min.Y(),
                contentRect.Max.X(), contentRect.Min.Y() + ScalePx(18.0f));
            DrawSectionHead(gui, theme, sectionRect, TEXT("Output"), TEXT("Recent logs"), false);
            const f32 entryGap  = ScalePx(8.0f);
            const f32 entryPadX = ScalePx(14.0f);
            const f32 entryPadY = ScalePx(10.0f);
            const f32 timeColW  = ScalePx(68.0f);
            const f32 levelColW = ScalePx(62.0f);
            const f32 lineHeight =
                gui.MeasureText(TEXT("Ag"), DebugGui::EDebugGuiFontRole::Small).Y();
            const f32 scrollBarWidth =
                ((theme.mScrollBarWidth > 0.0f) ? theme.mScrollBarWidth : 10.0f) * uiScale;
            const f32 scrollBarPad =
                ((theme.mScrollBarPadding > 0.0f) ? theme.mScrollBarPadding : 2.0f) * uiScale;
            const f32 thumbMinH =
                ((theme.mScrollBarThumbMinHeight > 0.0f) ? theme.mScrollBarThumbMinHeight : 14.0f)
                * uiScale;
            const FRect feedRect = MakeRect(
                contentRect.Min.X(), sectionRect.Max.Y(), contentRect.Max.X(), contentRect.Max.Y());
            const FRect scrollTrackRect = MakeRect(feedRect.Max.X() - scrollBarWidth - scrollBarPad,
                feedRect.Min.Y() + ScalePx(2.0f), feedRect.Max.X() - scrollBarPad,
                feedRect.Max.Y() - ScalePx(2.0f));
            const FRect feedContentRect = MakeRect(feedRect.Min.X(), feedRect.Min.Y(),
                scrollTrackRect.Min.X() - scrollBarPad, feedRect.Max.Y());
            struct FLogEntryDesc {
                FStringView mTime;
                FStringView mLevel;
                FStringView mText;
                bool        bWarn = false;
            };
            const FLogEntryDesc logEntries[3] = {
                { TEXT("12:01"), TEXT("INFO"),
                    TEXT(
                        "Editor initialized and workspace panels restored for the current session."),
                    false },
                { TEXT("12:03"), TEXT("INFO"),
                    TEXT(
                        "Asset index refreshed after theme update and panel style synchronization."),
                    false },
                { TEXT("12:04"), TEXT("WARN"),
                    TEXT(
                        "Viewport preview paused because runtime input is currently blocked by editor UI focus."),
                    true },
            };
            f32 totalHeight = 0.0f;
            for (const auto& logEntry : logEntries) {
                const f32 textX =
                    feedContentRect.Min.X() + entryPadX + timeColW + levelColW + ScalePx(28.0f);
                const f32  textWidth = feedContentRect.Max.X() - textX - entryPadX;
                const auto wrappedLines =
                    WrapTextToWidth(logEntry.mText, textWidth, glyphW * 0.84f);
                const f32 lineCount =
                    wrappedLines.IsEmpty() ? 1.0f : static_cast<f32>(wrappedLines.Size());
                totalHeight += entryPadY * 2.0f + lineHeight * lineCount + entryGap;
            }
            if (totalHeight > 0.0f) {
                totalHeight -= entryGap;
            }
            const f32 viewHeight      = feedContentRect.Max.Y() - feedContentRect.Min.Y();
            const f32 maxScrollY      = Core::Math::Max(0.0f, totalHeight - viewHeight);
            mOutputScrollY            = Clamp(mOutputScrollY, 0.0f, maxScrollY);
            const bool needsScrollBar = maxScrollY > 0.0f;
            if (!gui.IsMouseDown()) {
                mOutputScrollDragging = false;
            }
            if (!blockWorkspaceInput && needsScrollBar && gui.WasMousePressed()
                && IsInside(scrollTrackRect, mouse)) {
                const f32 trackH = scrollTrackRect.Max.Y() - scrollTrackRect.Min.Y();
                const f32 thumbHRaw =
                    (totalHeight > 0.0f) ? (viewHeight / totalHeight) * trackH : trackH;
                const f32   thumbH         = Clamp(thumbHRaw, thumbMinH, trackH);
                const f32   maxThumbTravel = Core::Math::Max(0.0f, trackH - thumbH);
                const f32   t         = (maxScrollY > 0.0f) ? (mOutputScrollY / maxScrollY) : 0.0f;
                const f32   thumbY    = scrollTrackRect.Min.Y() + maxThumbTravel * t;
                const FRect thumbRect = MakeRect(
                    scrollTrackRect.Min.X(), thumbY, scrollTrackRect.Max.X(), thumbY + thumbH);
                mOutputScrollDragging = true;
                mOutputScrollDragOffsetY =
                    IsInside(thumbRect, mouse) ? (mouse.Y() - thumbY) : (thumbH * 0.5f);
            }
            if (!blockWorkspaceInput && needsScrollBar && mOutputScrollDragging
                && gui.IsMouseDown()) {
                const f32 trackH = scrollTrackRect.Max.Y() - scrollTrackRect.Min.Y();
                const f32 thumbHRaw =
                    (totalHeight > 0.0f) ? (viewHeight / totalHeight) * trackH : trackH;
                const f32 thumbH         = Clamp(thumbHRaw, thumbMinH, trackH);
                const f32 maxThumbTravel = Core::Math::Max(0.0f, trackH - thumbH);
                const f32 minY           = scrollTrackRect.Min.Y();
                const f32 maxY           = scrollTrackRect.Max.Y() - thumbH;
                const f32 desiredY       = Clamp(mouse.Y() - mOutputScrollDragOffsetY, minY, maxY);
                const f32 t = (maxThumbTravel > 0.0f) ? ((desiredY - minY) / maxThumbTravel) : 0.0f;
                mOutputScrollY = t * maxScrollY;
            }
            f32        entryY       = feedContentRect.Min.Y() - mOutputScrollY;
            const auto drawLogEntry = [&](FStringView time, FStringView level, FStringView text,
                                          bool warn) {
                const f32 textX =
                    feedContentRect.Min.X() + entryPadX + timeColW + levelColW + ScalePx(28.0f);
                const f32  textWidth    = feedContentRect.Max.X() - textX - entryPadX;
                const auto wrappedLines = WrapTextToWidth(text, textWidth, glyphW * 0.84f);
                const f32  lineCount =
                    wrappedLines.IsEmpty() ? 1.0f : static_cast<f32>(wrappedLines.Size());
                const f32   entryHeight = entryPadY * 2.0f + lineHeight * lineCount;
                const FRect rect        = MakeRect(
                    feedContentRect.Min.X(), entryY, feedContentRect.Max.X(), entryY + entryHeight);
                gui.DrawRoundedRectFilled(
                    rect, editorTheme.mInsetSurface.mBg, editorTheme.mInsetSurface.mCornerRadius);
                gui.DrawTextStyled(FVector2f(rect.Min.X() + entryPadX, rect.Min.Y() + entryPadY),
                    colMutedText, time, DebugGui::EDebugGuiFontRole::Small);
                gui.DrawTextStyled(
                    FVector2f(rect.Min.X() + entryPadX + timeColW, rect.Min.Y() + entryPadY),
                    warn ? tabTheme.mUnderline : colMutedText, level,
                    DebugGui::EDebugGuiFontRole::Small);
                for (usize lineIndex = 0; lineIndex < wrappedLines.Size(); ++lineIndex) {
                    gui.DrawTextStyled(
                        FVector2f(textX,
                            rect.Min.Y() + entryPadY + lineHeight * static_cast<f32>(lineIndex)),
                        colText, wrappedLines[lineIndex].ToView(),
                        DebugGui::EDebugGuiFontRole::Small);
                }
                entryY = rect.Max.Y() + entryGap;
            };
            gui.PushClipRect(feedContentRect);
            for (const auto& logEntry : logEntries) {
                drawLogEntry(logEntry.mTime, logEntry.mLevel, logEntry.mText, logEntry.bWarn);
            }
            gui.PopClipRect();
            if (needsScrollBar) {
                const f32 trackH = scrollTrackRect.Max.Y() - scrollTrackRect.Min.Y();
                const f32 thumbHRaw =
                    (totalHeight > 0.0f) ? (viewHeight / totalHeight) * trackH : trackH;
                const f32   thumbH         = Clamp(thumbHRaw, thumbMinH, trackH);
                const f32   maxThumbTravel = Core::Math::Max(0.0f, trackH - thumbH);
                const f32   t         = (maxScrollY > 0.0f) ? (mOutputScrollY / maxScrollY) : 0.0f;
                const f32   thumbY    = scrollTrackRect.Min.Y() + maxThumbTravel * t;
                const FRect thumbRect = MakeRect(
                    scrollTrackRect.Min.X(), thumbY, scrollTrackRect.Max.X(), thumbY + thumbH);
                gui.DrawRoundedRectFilled(
                    scrollTrackRect, theme.mScrollBarTrackBg, ScalePx(999.0f));
                gui.DrawRoundedRectFilled(thumbRect,
                    mOutputScrollDragging ? theme.mScrollBarThumbActiveBg : theme.mScrollBarThumbBg,
                    ScalePx(999.0f));
            }
        };

        drawArea(areas[0]);
        drawArea(areas[1]);
        drawArea(areas[2]);
        drawArea(areas[3]);

        gui.DrawRectFilled(splitV1,
            (mActiveSplitter == 1 || IsInside(splitV1, mouse)) ? editorTheme.mSplitterHovered
                                                               : editorTheme.mSplitter);
        gui.DrawRectFilled(splitV2,
            (mActiveSplitter == 2 || IsInside(splitV2, mouse)) ? editorTheme.mSplitterHovered
                                                               : editorTheme.mSplitter);
        gui.DrawRectFilled(splitH,
            (mActiveSplitter == 3 || IsInside(splitH, mouse)) ? editorTheme.mSplitterHovered
                                                              : editorTheme.mSplitter);

        if (mDraggingPanel >= 0 && mDraggingPanel < static_cast<i32>(mPanels.Size())) {
            for (const auto& area : areas) {
                if (IsInside(area.Rect, mouse)) {
                    gui.DrawRectFilled(area.Rect, colDropHint);
                }
            }
            const auto& panel   = mPanels[static_cast<usize>(mDraggingPanel)];
            const FRect dragTag = MakeRect(mouse.X() + ScalePx(12.0f), mouse.Y() + ScalePx(12.0f),
                mouse.X() + ScalePx(12.0f) + TextWidth(panel.Name.ToView(), glyphW)
                    + ScalePx(14.0f),
                mouse.Y() + ScalePx(30.0f));
            gui.DrawRoundedRectFilled(dragTag, menuTheme.mPopupBg, menuTheme.mPopupRadius);
            gui.DrawTextStyled(
                FVector2f(dragTag.Min.X() + ScalePx(7.0f), dragTag.Min.Y() + ScalePx(4.0f)),
                menuTheme.mItemText, panel.Name.ToView(), DebugGui::EDebugGuiFontRole::Menu);
        }

        if (mOpenMenu >= 0 && mOpenMenu < 3) {
            const FRect anchor =
                mOpenMenuUseLegacyAnchor ? legacyMenus[mOpenMenu] : menus[mOpenMenu].Rect;
            const f32   itemH     = menuTheme.mItemHeight;
            const f32   menuW     = (mOpenMenu == 1) ? ScalePx(220.0f) : ScalePx(180.0f);
            const usize itemCount = (mOpenMenu == 1) ? 8U : 5U;
            const f32   popupPad  = mOpenMenuUseLegacyAnchor ? 0.0f : menuTheme.mPopupPad;
            const FRect dropRect  = MakeRect(anchor.Min.X(), anchor.Max.Y(), anchor.Min.X() + menuW,
                 anchor.Max.Y() + ScalePx(4.0f) + static_cast<f32>(itemCount) * itemH
                     + popupPad * 2.0f);
            gui.DrawRoundedRectFilled(dropRect, menuTheme.mPopupBg, menuTheme.mPopupRadius);

            auto itemRect = [&](usize idx) -> FRect {
                const f32 y0 = dropRect.Min.Y() + popupPad + static_cast<f32>(idx) * itemH;
                return MakeRect(
                    dropRect.Min.X() + popupPad, y0, dropRect.Max.X() - popupPad, y0 + itemH);
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
                drawMenuItem(itemRect(3), TEXT("Asset"), panelChecked(TEXT("Asset")), true,
                    [&]() { togglePanel(TEXT("Asset")); });
                drawMenuItem(itemRect(4), TEXT("Output"), panelChecked(TEXT("Output")), true,
                    [&]() { togglePanel(TEXT("Output")); });

                const bool statsShown =
                    (debugGuiSystem != nullptr) ? debugGuiSystem->IsStatsShown() : false;
                const bool consoleShown =
                    (debugGuiSystem != nullptr) ? debugGuiSystem->IsConsoleShown() : false;
                const bool cvarsShown =
                    (debugGuiSystem != nullptr) ? debugGuiSystem->IsCVarsShown() : false;
                drawMenuItem(itemRect(5), TEXT("Debug Stats"), statsShown,
                    debugGuiSystem != nullptr,
                    [&]() { debugGuiSystem->SetShowStats(!statsShown); });
                drawMenuItem(itemRect(6), TEXT("Debug Console"), consoleShown,
                    debugGuiSystem != nullptr,
                    [&]() { debugGuiSystem->SetShowConsole(!consoleShown); });
                drawMenuItem(itemRect(7), TEXT("Debug CVars"), cvarsShown,
                    debugGuiSystem != nullptr,
                    [&]() { debugGuiSystem->SetShowCVars(!cvarsShown); });
            } else {
                const bool isStopped = (mPlayState == EEditorUiPlayState::Stopped);
                const bool isRunning = (mPlayState == EEditorUiPlayState::Running);
                drawMenuItem(itemRect(0), TEXT("Play"), false, isStopped,
                    [&]() { queueCommand(EEditorUiCommand::Play); });
                drawMenuItem(itemRect(1), TEXT("Pause"), false, false,
                    [&]() { queueCommand(EEditorUiCommand::Pause); });
                drawMenuItem(itemRect(2), TEXT("Step"), false, false,
                    [&]() { queueCommand(EEditorUiCommand::Step); });
                drawMenuItem(itemRect(3), TEXT("Stop"), false, isRunning,
                    [&]() { queueCommand(EEditorUiCommand::Stop); });
                drawMenuItem(itemRect(4), TEXT("Simulate"), false, false, [] {});
            }

            if (mousePressed && !IsInside(dropRect, mouse) && !IsInside(anchor, mouse)) {
                mOpenMenu                = -1;
                mOpenMenuUseLegacyAnchor = false;
            }
        } else if (mousePressed && mouse.Y() > menuRect.Max.Y()) {
            mOpenMenu                = -1;
            mOpenMenuUseLegacyAnchor = false;
        }

        if (!blockWorkspaceInput && mousePressed) {
            mFocusedViewport = mStateStore.mViewportRequest.bHovered;
        }
        mStateStore.mViewportRequest.bFocused = mFocusedViewport;

        if (!mStateStore.mViewportRequest.bHasContent && previousViewportRequest.bHasContent) {
            const bool uiBlockingInput   = mStateStore.mViewportRequest.bUiBlockingInput;
            const bool hovered           = mStateStore.mViewportRequest.bHovered;
            const bool focused           = mStateStore.mViewportRequest.bFocused;
            mStateStore.mViewportRequest = previousViewportRequest;
            mStateStore.mViewportRequest.bUiBlockingInput = uiBlockingInput;
            mStateStore.mViewportRequest.bHovered         = hovered;
            mStateStore.mViewportRequest.bFocused         = focused;
        }
    }
} // namespace AltinaEngine::Editor::UI
