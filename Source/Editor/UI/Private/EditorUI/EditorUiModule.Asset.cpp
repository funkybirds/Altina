#include "EditorUI/EditorUiModule.h"

#include "Algorithm/Sort.h"
#include "DebugGui/DebugGui.h"
#include "Logging/Log.h"
#include "Math/Common.h"
#include "Math/Vector.h"
#include "Utility/Filesystem/FileSystem.h"

namespace AltinaEngine::Editor::UI {
    namespace {
        using ::AltinaEngine::Core::Container::FString;
        using ::AltinaEngine::Core::Container::FStringView;
        using ::AltinaEngine::Core::Container::TVector;
        using Core::Math::Clamp;
        using Core::Math::FVector2f;
        using Core::Utility::Filesystem::EnumerateDirectory;
        using Core::Utility::Filesystem::FDirectoryEntry;
        using Core::Utility::Filesystem::FPath;
        using DebugGui::FRect;

        constexpr f32      kGlyphW                     = 8.0f;
        constexpr u64      kAssetRefreshIntervalFrames = 60ULL;

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

        void DrawSectionHead(DebugGui::IDebugGui& gui, const DebugGui::FDebugGuiTheme& theme,
            const FRect& rect, FStringView label, FStringView note, bool accentLabel) {
            const auto& sectionTheme = theme.mEditor.mSections;
            const auto  labelColor =
                accentLabel ? theme.mEditor.mTabs.mUnderline : theme.mEditor.mPanelContentText;
            gui.DrawTextStyled(rect.Min, labelColor, label, DebugGui::EDebugGuiFontRole::Section);
            if (!note.IsEmpty()) {
                const auto noteSize = gui.MeasureText(note, DebugGui::EDebugGuiFontRole::Small);
                gui.DrawTextStyled(FVector2f(rect.Max.X() - noteSize.X(), rect.Min.Y() + 2.0f),
                    sectionTheme.mSecondaryText, note, DebugGui::EDebugGuiFontRole::Small);
            }
            const auto labelSize = gui.MeasureText(label, DebugGui::EDebugGuiFontRole::Section);
            const f32  lineY     = rect.Max.Y() - sectionTheme.mUnderlineThickness;
            gui.DrawRectFilled(
                MakeRect(rect.Min.X(), lineY, rect.Max.X(), rect.Max.Y()), sectionTheme.mLine);
            gui.DrawRectFilled(
                MakeRect(rect.Min.X(), lineY, rect.Min.X() + labelSize.X(), rect.Max.Y()),
                accentLabel ? theme.mEditor.mTabs.mUnderline
                            : theme.mEditor.mPanelContentMutedText);
        }

        void DrawScrollBar(DebugGui::IDebugGui& gui, const DebugGui::FDebugGuiTheme& theme,
            const FRect& trackRect, const FRect& thumbRect, bool hovered, bool active) {
            gui.DrawRoundedRectFilled(trackRect, theme.mScrollBarTrackBg, 999.0f);
            const auto thumbColor = active
                ? theme.mScrollBarThumbActiveBg
                : (hovered ? theme.mScrollBarThumbHoverBg : theme.mScrollBarThumbBg);
            gui.DrawRoundedRectFilled(thumbRect, thumbColor, 999.0f);
        }
    } // namespace
    auto FEditorUiModule::IsMetaPath(Core::Container::FStringView path) const -> bool {
        const FPath p(path);
        if (p.Extension() == FStringView(TEXT(".meta"))) {
            return true;
        }
        const auto filename = p.Filename();
        return filename.EndsWith(TEXT(".meta"));
    }

    auto FEditorUiModule::GetAssetDisplayName(Core::Container::FStringView path) const
        -> Core::Container::FString {
        const FPath p(path);
        const auto  filename = p.Filename();
        if (filename.IsEmpty()) {
            return FString(path);
        }
        return FString(filename);
    }

    auto FEditorUiModule::GetNodeIndexByPath(Core::Container::FStringView path) const -> i32 {
        auto it = mAssetNodeLookup.FindIt(FString(path));
        if (it == mAssetNodeLookup.end()) {
            return -1;
        }
        return it->second;
    }

    auto FEditorUiModule::TruncateAssetLabel(Core::Container::FStringView label, f32 maxWidth) const
        -> Core::Container::FString {
        if (label.IsEmpty()) {
            return FString();
        }

        const f32 glyphWidth = kGlyphW * ((mUiScale > 0.01f) ? mUiScale : 1.0f);
        const i32 maxChars   = static_cast<i32>(maxWidth / glyphWidth);
        if (maxChars <= 0) {
            return FString(TEXT("..."));
        }
        if (static_cast<i32>(label.Length()) <= maxChars) {
            return FString(label);
        }
        if (maxChars <= 3) {
            return FString(TEXT("..."));
        }

        FString out;
        out.Append(label.Substr(0, static_cast<usize>(maxChars - 3)));
        out.Append(TEXT("..."));
        return out;
    }

    void FEditorUiModule::EnsureNodeVisible(i32 nodeIndex) {
        i32 current = nodeIndex;
        while (current >= 0 && current < static_cast<i32>(mAssetNodes.Size())) {
            auto& node = mAssetNodes[static_cast<usize>(current)];
            if (node.mParentIndex >= 0) {
                node.mExpanded = true;
            }
            current = node.mParentIndex;
        }
    }

    void FEditorUiModule::BuildAssetItemsForCurrentFolder() {
        mAssetItems.Clear();
        if (mCurrentAssetPath.IsEmptyString()) {
            return;
        }

        const FPath currentPath(mCurrentAssetPath);
        const FPath rootPath(mAssetRootPath);
        if (!mAssetRootPath.IsEmptyString()) {
            const auto currentNormalized = currentPath.Normalized().GetString();
            const auto rootNormalized    = rootPath.Normalized().GetString();
            if (currentNormalized != rootNormalized) {
                const auto parentPath = currentPath.ParentPath().Normalized().GetString();
                if (!parentPath.IsEmptyString() && parentPath.StartsWith(rootNormalized.ToView())) {
                    FAssetItem upItem{};
                    upItem.mPath       = parentPath;
                    upItem.mName       = FString(TEXT(".."));
                    upItem.mType       = EAssetItemType::Directory;
                    upItem.mNavigateUp = true;
                    mAssetItems.PushBack(Move(upItem));
                }
            }
        }

        TVector<FDirectoryEntry> entries;
        if (!EnumerateDirectory(FPath(mCurrentAssetPath), false, entries)) {
            return;
        }

        for (const auto& entry : entries) {
            const auto normalized = entry.Path.Normalized().GetString();
            if (IsMetaPath(normalized.ToView())) {
                continue;
            }

            FAssetItem item{};
            item.mPath = normalized;
            item.mName = GetAssetDisplayName(normalized.ToView());
            item.mType = entry.IsDirectory ? EAssetItemType::Directory : EAssetItemType::File;
            mAssetItems.PushBack(Move(item));
        }

        Core::Algorithm::Sort(mAssetItems, [](const FAssetItem& lhs, const FAssetItem& rhs) {
            if (lhs.mNavigateUp != rhs.mNavigateUp) {
                return lhs.mNavigateUp;
            }
            if (lhs.mType != rhs.mType) {
                return lhs.mType == EAssetItemType::Directory;
            }
            return lhs.mName < rhs.mName.ToView();
        });
    }

    void FEditorUiModule::RefreshAssetCache(bool force) {
        if (mAssetRootPath.IsEmptyString()) {
            return;
        }
        if (!force && !mAssetNeedsRefresh
            && (mStateStore.mFrameCounter - mAssetLastRefreshFrame) < kAssetRefreshIntervalFrames) {
            return;
        }

        mAssetNeedsRefresh     = false;
        mAssetLastRefreshFrame = mStateStore.mFrameCounter;

        Core::Container::THashMap<FString, bool> oldExpanded;
        for (const auto& node : mAssetNodes) {
            oldExpanded[node.mPath] = node.mExpanded;
        }

        TVector<FDirectoryEntry> entries;
        mAssetNodes.Clear();
        mAssetNodeLookup.Clear();

        FAssetNode root{};
        root.mPath = FPath(mAssetRootPath).Normalized().GetString();
        root.mName = GetAssetDisplayName(root.mPath.ToView());
        if (root.mName.IsEmptyString()) {
            root.mName = root.mPath;
        }
        auto rootExpandedIt = oldExpanded.FindIt(root.mPath);
        root.mExpanded      = (rootExpandedIt != oldExpanded.end()) ? rootExpandedIt->second : true;
        root.mVisible       = true;
        mAssetNodeLookup[root.mPath] = 0;
        mAssetNodes.PushBack(Move(root));

        if (EnumerateDirectory(FPath(mAssetRootPath), true, entries)) {
            TVector<FPath> dirs;
            for (const auto& entry : entries) {
                if (!entry.IsDirectory) {
                    continue;
                }
                const auto normalized = entry.Path.Normalized();
                if (IsMetaPath(normalized.ToView())) {
                    continue;
                }
                dirs.PushBack(normalized);
            }

            Core::Algorithm::Sort(dirs, [](const FPath& lhs, const FPath& rhs) {
                const auto lhsLen = lhs.GetString().Length();
                const auto rhsLen = rhs.GetString().Length();
                if (lhsLen != rhsLen) {
                    return lhsLen < rhsLen;
                }
                return lhs.GetString() < rhs.GetString().ToView();
            });

            for (const auto& dir : dirs) {
                const auto dirPath = dir.GetString();
                if (mAssetNodeLookup.FindIt(dirPath) != mAssetNodeLookup.end()) {
                    continue;
                }
                const auto parentPath = dir.ParentPath().Normalized().GetString();
                auto       parentIt   = mAssetNodeLookup.FindIt(parentPath);
                if (parentIt == mAssetNodeLookup.end()) {
                    continue;
                }
                const i32 parentIndex = parentIt->second;
                if (parentIndex < 0 || parentIndex >= static_cast<i32>(mAssetNodes.Size())) {
                    continue;
                }

                FAssetNode node{};
                node.mPath        = dirPath;
                node.mName        = GetAssetDisplayName(dirPath.ToView());
                node.mParentIndex = parentIndex;
                auto expandedIt   = oldExpanded.FindIt(node.mPath);
                node.mExpanded    = (expandedIt != oldExpanded.end()) ? expandedIt->second : false;

                const i32 index              = static_cast<i32>(mAssetNodes.Size());
                mAssetNodeLookup[node.mPath] = index;
                mAssetNodes.PushBack(Move(node));
                mAssetNodes[static_cast<usize>(parentIndex)].mChildren.PushBack(index);
            }

            for (auto& node : mAssetNodes) {
                if (!node.mChildren.IsEmpty()) {
                    TVector<i32> filtered;
                    filtered.Reserve(node.mChildren.Size());
                    for (const i32 childIndex : node.mChildren) {
                        if (childIndex >= 0 && childIndex < static_cast<i32>(mAssetNodes.Size())) {
                            filtered.PushBack(childIndex);
                        }
                    }
                    node.mChildren = Move(filtered);
                }
                Core::Algorithm::Sort(node.mChildren, [this](i32 lhs, i32 rhs) {
                    const auto& lhsNode = mAssetNodes[static_cast<usize>(lhs)];
                    const auto& rhsNode = mAssetNodes[static_cast<usize>(rhs)];
                    return lhsNode.mName < rhsNode.mName.ToView();
                });
            }
        }

        if (mCurrentAssetPath.IsEmptyString()
            || GetNodeIndexByPath(mCurrentAssetPath.ToView()) < 0) {
            mCurrentAssetPath = mAssetRootPath;
        }
        const i32 currentNodeIndex = GetNodeIndexByPath(mCurrentAssetPath.ToView());
        if (currentNodeIndex > 0) {
            EnsureNodeVisible(currentNodeIndex);
        }
        BuildAssetItemsForCurrentFolder();
    }

    void FEditorUiModule::OpenPathInAssetView(
        Core::Container::FStringView path, EAssetItemType type) {
        if (type == EAssetItemType::Directory) {
            mCurrentAssetPath   = FPath(path).Normalized().GetString();
            mSelectedAssetPath  = mCurrentAssetPath;
            mSelectedAssetType  = type;
            const i32 nodeIndex = GetNodeIndexByPath(mCurrentAssetPath.ToView());
            if (nodeIndex >= 0) {
                EnsureNodeVisible(nodeIndex);
                if (mAssetNodes[static_cast<usize>(nodeIndex)].mParentIndex >= 0) {
                    mAssetNodes[static_cast<usize>(nodeIndex)].mExpanded = true;
                }
            }
            BuildAssetItemsForCurrentFolder();
            return;
        }
        mSelectedAssetPath = FString(path);
        mSelectedAssetType = type;
    }

    void FEditorUiModule::DrawHierarchyPanel(DebugGui::IDebugGui& gui,
        const DebugGui::FRect& contentRect, const Core::Math::FVector2f& mouse,
        bool blockWorkspaceInput) {
        const f32                      uiScale = (mUiScale > 0.01f) ? mUiScale : 1.0f;
        const auto                     ScalePx = [uiScale](f32 value) { return value * uiScale; };
        const DebugGui::FDebugGuiTheme theme =
            (mDebugGuiSystem != nullptr) ? mDebugGuiSystem->GetTheme() : DebugGui::FDebugGuiTheme{};
        const auto  colText      = theme.mEditor.mPanelContentText;
        const auto  colMutedText = theme.mEditor.mPanelContentMutedText;

        const FRect sectionRect = MakeRect(contentRect.Min.X(), contentRect.Min.Y(),
            contentRect.Max.X(), contentRect.Min.Y() + ScalePx(20.0f));
        if (mHierarchySnapshot.mGameObjects.IsEmpty()) {
            DrawSectionHead(gui, theme, sectionRect, TEXT("World"), TEXT("No active world"), false);
            return;
        }

        Core::Container::FString worldCountLabel;
        worldCountLabel.AppendNumber(static_cast<i32>(mHierarchySnapshot.mGameObjects.Size()));
        worldCountLabel.Append(TEXT(" live objects"));
        DrawSectionHead(gui, theme, sectionRect, TEXT("World"), worldCountLabel.ToView(), false);

        const FRect treeRect = MakeRect(contentRect.Min.X(), sectionRect.Max.Y() + ScalePx(2.0f),
            contentRect.Max.X(), contentRect.Max.Y());
        const f32   rowHeight =
            ((theme.mTreeRowHeight > 0.0f) ? theme.mTreeRowHeight : 18.0f) * uiScale;
        const f32 rowStep =
            rowHeight + ((theme.mItemSpacingY > 0.0f) ? theme.mItemSpacingY : 4.0f) * uiScale;
        const f32 scrollBarWidth =
            ((theme.mScrollBarWidth > 0.0f) ? theme.mScrollBarWidth : 10.0f) * uiScale;
        const f32 scrollBarPad =
            ((theme.mScrollBarPadding > 0.0f) ? theme.mScrollBarPadding : 2.0f) * uiScale;
        const f32 thumbMinH =
            ((theme.mScrollBarThumbMinHeight > 0.0f) ? theme.mScrollBarThumbMinHeight : 14.0f)
            * uiScale;

        i32 visibleRowCount = 0;
        {
            TVector<i32> countStack;
            for (isize i = static_cast<isize>(mHierarchyRoots.Size()) - 1; i >= 0; --i) {
                countStack.PushBack(mHierarchyRoots[static_cast<usize>(i)]);
                if (i == 0) {
                    break;
                }
            }
            while (!countStack.IsEmpty()) {
                const i32 objectIndex = countStack.Back();
                countStack.PopBack();
                if (objectIndex < 0
                    || objectIndex >= static_cast<i32>(mHierarchySnapshot.mGameObjects.Size())) {
                    continue;
                }
                const auto& object =
                    mHierarchySnapshot.mGameObjects[static_cast<usize>(objectIndex)];
                visibleRowCount += 1;
                const auto uuid     = MakeGameObjectUuid(object.mId);
                const auto keyIt    = mHierarchyExpanded.FindIt(uuid);
                const bool expanded = (keyIt != mHierarchyExpanded.end()) ? keyIt->second : false;
                if (!expanded) {
                    continue;
                }
                visibleRowCount += static_cast<i32>(object.mComponents.Size());
                const auto& children = mHierarchyChildren[static_cast<usize>(objectIndex)];
                for (isize child = static_cast<isize>(children.Size()) - 1; child >= 0; --child) {
                    countStack.PushBack(children[static_cast<usize>(child)]);
                    if (child == 0) {
                        break;
                    }
                }
            }
        }

        const f32 viewHeight =
            Core::Math::Max(0.0f, treeRect.Max.Y() - treeRect.Min.Y() - ScalePx(4.0f));
        const f32 totalHeight = static_cast<f32>(visibleRowCount) * rowStep;
        const f32 maxScrollY  = Core::Math::Max(0.0f, totalHeight - viewHeight);
        mHierarchyScrollY     = Clamp(mHierarchyScrollY, 0.0f, maxScrollY);

        const bool needsScrollBar = (maxScrollY > 0.0f)
            && (treeRect.Max.X() - treeRect.Min.X()
                > (scrollBarWidth + scrollBarPad + ScalePx(24.0f)));
        const FRect scrollTrackRect = needsScrollBar
            ? MakeRect(treeRect.Max.X() - scrollBarWidth - scrollBarPad,
                  treeRect.Min.Y() + ScalePx(2.0f), treeRect.Max.X() - scrollBarPad,
                  treeRect.Max.Y() - ScalePx(2.0f))
            : MakeRect(0.0f, 0.0f, 0.0f, 0.0f);
        const FRect treeContentRect = needsScrollBar
            ? MakeRect(treeRect.Min.X(), treeRect.Min.Y(), scrollTrackRect.Min.X() - scrollBarPad,
                  treeRect.Max.Y())
            : treeRect;

        if (!gui.IsMouseDown()) {
            mHierarchyScrollDragging = false;
        }
        if (!blockWorkspaceInput && needsScrollBar && gui.WasMousePressed()
            && IsInside(scrollTrackRect, mouse)) {
            const f32 trackH = scrollTrackRect.Max.Y() - scrollTrackRect.Min.Y();
            const f32 thumbHRaw =
                (totalHeight > 0.0f) ? (viewHeight / totalHeight) * trackH : trackH;
            const f32   thumbH         = Clamp(thumbHRaw, thumbMinH, trackH);
            const f32   maxThumbTravel = Core::Math::Max(0.0f, trackH - thumbH);
            const f32   t      = (maxScrollY > 0.0f) ? (mHierarchyScrollY / maxScrollY) : 0.0f;
            const f32   thumbY = scrollTrackRect.Min.Y() + maxThumbTravel * t;
            const FRect thumbRect =
                MakeRect(scrollTrackRect.Min.X(), thumbY, scrollTrackRect.Max.X(), thumbY + thumbH);
            mHierarchyScrollDragging = true;
            mHierarchyScrollDragOffsetY =
                IsInside(thumbRect, mouse) ? (mouse.Y() - thumbY) : (thumbH * 0.5f);
        }
        if (!blockWorkspaceInput && needsScrollBar && mHierarchyScrollDragging
            && gui.IsMouseDown()) {
            const f32 trackH = scrollTrackRect.Max.Y() - scrollTrackRect.Min.Y();
            const f32 thumbHRaw =
                (totalHeight > 0.0f) ? (viewHeight / totalHeight) * trackH : trackH;
            const f32 thumbH         = Clamp(thumbHRaw, thumbMinH, trackH);
            const f32 maxThumbTravel = Core::Math::Max(0.0f, trackH - thumbH);
            const f32 minY           = scrollTrackRect.Min.Y();
            const f32 maxY           = scrollTrackRect.Max.Y() - thumbH;
            const f32 desiredY       = Clamp(mouse.Y() - mHierarchyScrollDragOffsetY, minY, maxY);
            const f32 t = (maxThumbTravel > 0.0f) ? ((desiredY - minY) / maxThumbTravel) : 0.0f;
            mHierarchyScrollY = t * maxScrollY;
        }

        gui.PushClipRect(treeContentRect);
        gui.SetCursorPos(FVector2f(treeContentRect.Min.X() + ScalePx(2.0f),
            treeContentRect.Min.Y() + ScalePx(4.0f) - mHierarchyScrollY));

        TVector<i32> indexStack;
        TVector<u32> depthStack;
        for (isize i = static_cast<isize>(mHierarchyRoots.Size()) - 1; i >= 0; --i) {
            indexStack.PushBack(mHierarchyRoots[static_cast<usize>(i)]);
            depthStack.PushBack(0U);
            if (i == 0) {
                break;
            }
        }

        while (!indexStack.IsEmpty()) {
            const i32 objectIndex = indexStack.Back();
            indexStack.PopBack();
            const u32 depth = depthStack.Back();
            depthStack.PopBack();
            if (objectIndex < 0
                || objectIndex >= static_cast<i32>(mHierarchySnapshot.mGameObjects.Size())) {
                continue;
            }

            const auto& object   = mHierarchySnapshot.mGameObjects[static_cast<usize>(objectIndex)];
            const auto& children = mHierarchyChildren[static_cast<usize>(objectIndex)];
            const auto  uuid     = MakeGameObjectUuid(object.mId);
            const auto  keyIt    = mHierarchyExpanded.FindIt(uuid);
            const bool  expanded = (keyIt != mHierarchyExpanded.end()) ? keyIt->second : false;
            const bool  hasChildren = !children.IsEmpty() || !object.mComponents.IsEmpty();

            DebugGui::FTreeViewItemDesc objectDesc{};
            objectDesc.mLabel       = object.mName.ToView();
            objectDesc.mDepth       = depth;
            objectDesc.mSelected    = IsGameObjectSelected(object.mId);
            objectDesc.mExpanded    = expanded;
            objectDesc.mHasChildren = hasChildren;

            const auto objectResult = gui.TreeViewItem(objectDesc);
            if (!blockWorkspaceInput && objectResult.mClicked) {
                SelectGameObject(object.mId);
            }
            if (!blockWorkspaceInput && objectResult.mToggleExpanded && hasChildren) {
                mHierarchyExpanded[uuid] = !expanded;
            }

            const bool currentExpanded =
                (mHierarchyExpanded.FindIt(uuid) != mHierarchyExpanded.end())
                ? mHierarchyExpanded[uuid]
                : false;
            if (!currentExpanded) {
                continue;
            }

            for (isize child = static_cast<isize>(children.Size()) - 1; child >= 0; --child) {
                indexStack.PushBack(children[static_cast<usize>(child)]);
                depthStack.PushBack(depth + 1U);
                if (child == 0) {
                    break;
                }
            }

            for (usize c = 0; c < object.mComponents.Size(); ++c) {
                const auto&                 component = object.mComponents[c];
                DebugGui::FTreeViewItemDesc componentDesc{};
                componentDesc.mLabel       = component.mName.ToView();
                componentDesc.mDepth       = depth + 1U;
                componentDesc.mSelected    = IsComponentSelected(component.mId);
                componentDesc.mExpanded    = false;
                componentDesc.mHasChildren = false;
                const auto componentResult = gui.TreeViewItem(componentDesc);
                if (!blockWorkspaceInput && componentResult.mClicked) {
                    SelectComponent(component.mId);
                }
            }
        }

        gui.PopClipRect();
        if (needsScrollBar) {
            const f32 trackH = scrollTrackRect.Max.Y() - scrollTrackRect.Min.Y();
            const f32 thumbHRaw =
                (totalHeight > 0.0f) ? (viewHeight / totalHeight) * trackH : trackH;
            const f32   thumbH         = Clamp(thumbHRaw, thumbMinH, trackH);
            const f32   maxThumbTravel = Core::Math::Max(0.0f, trackH - thumbH);
            const f32   t      = (maxScrollY > 0.0f) ? (mHierarchyScrollY / maxScrollY) : 0.0f;
            const f32   thumbY = scrollTrackRect.Min.Y() + maxThumbTravel * t;
            const FRect thumbRect =
                MakeRect(scrollTrackRect.Min.X(), thumbY, scrollTrackRect.Max.X(), thumbY + thumbH);
            const bool thumbHovered = IsInside(thumbRect, mouse);
            DrawScrollBar(
                gui, theme, scrollTrackRect, thumbRect, thumbHovered, mHierarchyScrollDragging);
        }
    }

    void FEditorUiModule::DrawInspectorPanel(
        DebugGui::IDebugGui& gui, const DebugGui::FRect& contentRect) const {
        const f32                      uiScale = (mUiScale > 0.01f) ? mUiScale : 1.0f;
        const auto                     ScalePx = [uiScale](f32 value) { return value * uiScale; };
        const DebugGui::FDebugGuiTheme theme =
            (mDebugGuiSystem != nullptr) ? mDebugGuiSystem->GetTheme() : DebugGui::FDebugGuiTheme{};
        const auto   colText       = theme.mEditor.mPanelContentText;
        const auto   colMutedText  = theme.mEditor.mPanelContentMutedText;
        const FRect  sectionRect   = MakeRect(contentRect.Min.X(), contentRect.Min.Y(),
               contentRect.Max.X(), contentRect.Min.Y() + ScalePx(20.0f));
        const TChar* selectionType = TEXT("None");
        if (mSelection.mType == EEditorSelectionType::GameObject) {
            selectionType = TEXT("Actor");
        } else if (mSelection.mType == EEditorSelectionType::Component) {
            selectionType = TEXT("Component");
        }
        DrawSectionHead(gui, theme, sectionRect, TEXT("Selected"), selectionType, true);

        const FRect cardRect = MakeRect(contentRect.Min.X(), sectionRect.Max.Y() + ScalePx(3.0f),
            contentRect.Max.X(), contentRect.Max.Y());
        gui.DrawRoundedRectFilled(cardRect, DebugGui::MakeColor32(255, 255, 255, 168),
            theme.mEditor.mInsetSurface.mCornerRadius);
        const f32 x = cardRect.Min.X() + ScalePx(14.0f);
        f32       y = cardRect.Min.Y() + ScalePx(16.0f);

        if (mSelection.mType == EEditorSelectionType::None) {
            gui.DrawTextStyled(FVector2f(x, y), colText, TEXT("Nothing selected"),
                DebugGui::EDebugGuiFontRole::Section);
            gui.DrawTextStyled(FVector2f(x, y + ScalePx(24.0f)), colMutedText,
                TEXT("Select a GameObject or Component."), DebugGui::EDebugGuiFontRole::Body);
            return;
        }

        gui.DrawTextStyled(FVector2f(x, y), colText, mSelection.mName.ToView(),
            DebugGui::EDebugGuiFontRole::Section);
        y += ScalePx(22.0f);
        gui.DrawTextStyled(FVector2f(x, y), colMutedText, mSelection.mUuid.ToView(),
            DebugGui::EDebugGuiFontRole::Small);
        y += ScalePx(24.0f);

        const f32 rowPadX      = ScalePx(6.0f);
        const f32 labelInsetX  = ScalePx(7.0f);
        const f32 labelColumnW = ScalePx(66.0f);
        const f32 valueInsetX  = ScalePx(8.0f);
        const f32 valueTextWidth =
            (cardRect.Max.X() - ScalePx(14.0f)) - (x + labelColumnW + valueInsetX) - rowPadX;
        const auto drawFieldRow = [&](FStringView label, FStringView value) {
            const FRect rowRect =
                MakeRect(x, y, cardRect.Max.X() - ScalePx(14.0f), y + ScalePx(28.0f));
            gui.DrawRoundedRectFilled(rowRect, DebugGui::MakeColor32(255, 255, 255, 174),
                theme.mEditor.mInsetSurface.mCornerRadius);
            gui.DrawTextStyled(
                FVector2f(rowRect.Min.X() + labelInsetX, rowRect.Min.Y() + ScalePx(6.0f)),
                colMutedText, label, DebugGui::EDebugGuiFontRole::Small);
            const auto wrappedLines = TruncateAssetLabel(value, valueTextWidth).ToView();
            gui.DrawTextStyled(FVector2f(rowRect.Min.X() + labelColumnW + valueInsetX,
                                   rowRect.Min.Y() + ScalePx(5.0f)),
                colText, wrappedLines, DebugGui::EDebugGuiFontRole::Body);
            y += ScalePx(32.0f);
        };

        drawFieldRow(TEXT("Selection"), selectionType);
        drawFieldRow(TEXT("Name"), mSelection.mName.ToView());
        drawFieldRow(TEXT("UUID"), mSelection.mUuid.ToView());

        if (mSelection.mType == EEditorSelectionType::Component) {
            drawFieldRow(TEXT("Type"), mSelection.mTypeName.ToView());
            drawFieldRow(TEXT("Namespace"),
                mSelection.mTypeNamespace.IsEmptyString() ? FStringView(TEXT("(global)"))
                                                          : mSelection.mTypeNamespace.ToView());
        }

        gui.DrawTextStyled(FVector2f(x, y + ScalePx(4.0f)), colMutedText,
            TEXT("Properties (Coming Soon)"), DebugGui::EDebugGuiFontRole::Small);
    }

    void FEditorUiModule::DrawAssetPanel(DebugGui::IDebugGui& gui,
        const DebugGui::FRect& contentRect, const Core::Math::FVector2f& mouse,
        bool blockWorkspaceInput) {
        const f32                uiScale = (mUiScale > 0.01f) ? mUiScale : 1.0f;
        const auto               ScalePx = [uiScale](f32 value) { return value * uiScale; };
        DebugGui::FDebugGuiTheme theme{};
        if (mDebugGuiSystem != nullptr) {
            theme = mDebugGuiSystem->GetTheme();
        }
        const auto colPanelBg        = theme.mEditor.mInsetSurface.mBg;
        const auto colBorder         = theme.mEditor.mInsetSurface.mBorder;
        const auto colText           = theme.mEditor.mPanelContentText;
        const auto colMutedText      = theme.mEditor.mPanelContentMutedText;
        const auto colMenuBg         = theme.mEditor.mMenu.mPopupBg;
        const auto colMenuHover      = theme.mEditor.mMenu.mItemBgHovered;
        const auto HashAssetItemPath = [](Core::Container::FStringView path) -> u64 {
            constexpr u64 kOffset = 1469598103934665603ULL;
            constexpr u64 kPrime  = 1099511628211ULL;
            u64           h       = kOffset;
            for (usize i = 0; i < path.Length(); ++i) {
                h ^= static_cast<u64>(static_cast<u32>(path[i]));
                h *= kPrime;
            }
            return h;
        };

        const f32 panelInset    = ScalePx(2.0f);
        const f32 panelWidth    = contentRect.Max.X() - contentRect.Min.X();
        const f32 minTreeWidth  = ScalePx(120.0f);
        const f32 minGridWidth  = ScalePx(180.0f);
        const f32 splitterWidth = ScalePx(8.0f);
        const f32 minSplitX     = contentRect.Min.X() + panelInset + minTreeWidth;
        const f32 maxSplitRaw   = contentRect.Max.X() - panelInset - minGridWidth - splitterWidth;
        const f32 maxSplitX     = (maxSplitRaw > minSplitX) ? maxSplitRaw : minSplitX;
        const f32 splitX =
            Clamp(contentRect.Min.X() + panelInset + panelWidth * mAssetTreeSplitRatio, minSplitX,
                maxSplitX);
        const FRect treeRect     = MakeRect(contentRect.Min.X() + panelInset,
                contentRect.Min.Y() + panelInset, splitX, contentRect.Max.Y() - panelInset);
        const FRect splitterRect = MakeRect(splitX, contentRect.Min.Y() + panelInset,
            splitX + splitterWidth, contentRect.Max.Y() - panelInset);
        const FRect gridRect     = MakeRect(splitterRect.Max.X(), contentRect.Min.Y() + panelInset,
                contentRect.Max.X() - panelInset, contentRect.Max.Y() - panelInset);

        if (!blockWorkspaceInput && gui.WasMousePressed() && IsInside(splitterRect, mouse)) {
            mAssetSplitterActive = true;
        }
        if (!gui.IsMouseDown()) {
            mAssetSplitterActive = false;
        }
        if (!blockWorkspaceInput && mAssetSplitterActive && gui.IsMouseDown()) {
            const f32 newSplit   = Clamp(mouse.X() - splitterWidth * 0.5f, minSplitX, maxSplitX);
            mAssetTreeSplitRatio = (newSplit - contentRect.Min.X() - panelInset) / panelWidth;
        }

        gui.DrawRoundedRectFilled(treeRect, colPanelBg, theme.mEditor.mInsetSurface.mCornerRadius);
        gui.DrawRoundedRectFilled(splitterRect,
            (mAssetSplitterActive || IsInside(splitterRect, mouse)) ? theme.mEditor.mSplitterHovered
                                                                    : theme.mEditor.mSplitter,
            ScalePx(999.0f));
        gui.DrawRoundedRectFilled(gridRect, colPanelBg, theme.mEditor.mInsetSurface.mCornerRadius);

        const FRect treeSectionRect =
            MakeRect(treeRect.Min.X() + ScalePx(14.0f), treeRect.Min.Y() + ScalePx(12.0f),
                treeRect.Max.X() - ScalePx(14.0f), treeRect.Min.Y() + ScalePx(30.0f));
        const FRect gridSectionRect =
            MakeRect(gridRect.Min.X() + ScalePx(14.0f), gridRect.Min.Y() + ScalePx(12.0f),
                gridRect.Max.X() - ScalePx(14.0f), gridRect.Min.Y() + ScalePx(30.0f));
        DrawSectionHead(
            gui, theme, treeSectionRect, TEXT("Folders"), TEXT("Project assets"), false);
        DrawSectionHead(
            gui, theme, gridSectionRect, TEXT("Browser"), mCurrentAssetPath.ToView(), true);

        const f32 treeRowHeight =
            ((theme.mTreeRowHeight > 0.0f) ? theme.mTreeRowHeight : 18.0f) * uiScale;
        const f32 treeRowStep =
            treeRowHeight + ((theme.mItemSpacingY > 0.0f) ? theme.mItemSpacingY : 4.0f) * uiScale;
        const f32 scrollBarWidth =
            ((theme.mScrollBarWidth > 0.0f) ? theme.mScrollBarWidth : 10.0f) * uiScale;
        const f32 scrollBarPad =
            ((theme.mScrollBarPadding > 0.0f) ? theme.mScrollBarPadding : 2.0f) * uiScale;
        const f32 thumbMinH =
            ((theme.mScrollBarThumbMinHeight > 0.0f) ? theme.mScrollBarThumbMinHeight : 14.0f)
            * uiScale;

        i32 visibleNodeCount = 0;
        if (!mAssetNodes.IsEmpty()) {
            TVector<i32> countStack;
            countStack.PushBack(0);
            while (!countStack.IsEmpty()) {
                const i32 nodeIndex = countStack.Back();
                countStack.PopBack();
                if (nodeIndex < 0 || nodeIndex >= static_cast<i32>(mAssetNodes.Size())) {
                    continue;
                }
                ++visibleNodeCount;
                const auto& node = mAssetNodes[static_cast<usize>(nodeIndex)];
                if (!node.mExpanded) {
                    continue;
                }
                for (isize child = static_cast<isize>(node.mChildren.Size()) - 1; child >= 0;
                    --child) {
                    countStack.PushBack(node.mChildren[static_cast<usize>(child)]);
                    if (child == 0) {
                        break;
                    }
                }
            }
        }

        const f32 treeViewHeight =
            Core::Math::Max(0.0f, treeRect.Max.Y() - treeSectionRect.Max.Y() - ScalePx(8.0f));
        const f32 treeTotalHeight     = static_cast<f32>(visibleNodeCount) * treeRowStep;
        const f32 treeMaxScrollY      = Core::Math::Max(0.0f, treeTotalHeight - treeViewHeight);
        mAssetTreeScrollY             = Clamp(mAssetTreeScrollY, 0.0f, treeMaxScrollY);
        const bool treeNeedsScrollBar = (treeMaxScrollY > 0.0f)
            && (treeRect.Max.X() - treeRect.Min.X()
                > (scrollBarWidth + scrollBarPad + ScalePx(24.0f)));
        const FRect treeScrollTrackRect = treeNeedsScrollBar
            ? MakeRect(treeRect.Max.X() - scrollBarWidth - scrollBarPad,
                  treeSectionRect.Max.Y() + ScalePx(2.0f), treeRect.Max.X() - scrollBarPad,
                  treeRect.Max.Y() - ScalePx(8.0f))
            : MakeRect(0.0f, 0.0f, 0.0f, 0.0f);
        const FRect treeContentRect     = treeNeedsScrollBar
                ? MakeRect(treeRect.Min.X() + ScalePx(8.0f), treeSectionRect.Max.Y() + ScalePx(2.0f),
                      treeScrollTrackRect.Min.X() - scrollBarPad, treeRect.Max.Y() - ScalePx(8.0f))
                : MakeRect(treeRect.Min.X() + ScalePx(8.0f), treeSectionRect.Max.Y() + ScalePx(2.0f),
                      treeRect.Max.X() - ScalePx(8.0f), treeRect.Max.Y() - ScalePx(8.0f));

        if (!gui.IsMouseDown()) {
            mAssetTreeScrollDragging = false;
        }
        if (!blockWorkspaceInput && treeNeedsScrollBar && gui.WasMousePressed()
            && IsInside(treeScrollTrackRect, mouse)) {
            const f32 trackH = treeScrollTrackRect.Max.Y() - treeScrollTrackRect.Min.Y();
            const f32 thumbHRaw =
                (treeTotalHeight > 0.0f) ? (treeViewHeight / treeTotalHeight) * trackH : trackH;
            const f32   thumbH         = Clamp(thumbHRaw, thumbMinH, trackH);
            const f32   maxThumbTravel = Core::Math::Max(0.0f, trackH - thumbH);
            const f32   t = (treeMaxScrollY > 0.0f) ? (mAssetTreeScrollY / treeMaxScrollY) : 0.0f;
            const f32   thumbY    = treeScrollTrackRect.Min.Y() + maxThumbTravel * t;
            const FRect thumbRect = MakeRect(
                treeScrollTrackRect.Min.X(), thumbY, treeScrollTrackRect.Max.X(), thumbY + thumbH);
            mAssetTreeScrollDragging = true;
            mAssetTreeScrollDragOffsetY =
                IsInside(thumbRect, mouse) ? (mouse.Y() - thumbY) : (thumbH * 0.5f);
        }
        if (!blockWorkspaceInput && treeNeedsScrollBar && mAssetTreeScrollDragging
            && gui.IsMouseDown()) {
            const f32 trackH = treeScrollTrackRect.Max.Y() - treeScrollTrackRect.Min.Y();
            const f32 thumbHRaw =
                (treeTotalHeight > 0.0f) ? (treeViewHeight / treeTotalHeight) * trackH : trackH;
            const f32 thumbH         = Clamp(thumbHRaw, thumbMinH, trackH);
            const f32 maxThumbTravel = Core::Math::Max(0.0f, trackH - thumbH);
            const f32 minY           = treeScrollTrackRect.Min.Y();
            const f32 maxY           = treeScrollTrackRect.Max.Y() - thumbH;
            const f32 desiredY       = Clamp(mouse.Y() - mAssetTreeScrollDragOffsetY, minY, maxY);
            const f32 t = (maxThumbTravel > 0.0f) ? ((desiredY - minY) / maxThumbTravel) : 0.0f;
            mAssetTreeScrollY = t * treeMaxScrollY;
        }

        gui.PushClipRect(treeContentRect);
        gui.SetCursorPos(
            FVector2f(treeContentRect.Min.X(), treeContentRect.Min.Y() - mAssetTreeScrollY));

        TVector<i32> drawStack;
        TVector<u32> depthStack;
        FString      pendingTreeOpenPath;
        bool         bHasPendingTreeOpen = false;
        if (!mAssetNodes.IsEmpty()) {
            drawStack.PushBack(0);
            depthStack.PushBack(0U);
        }
        while (!drawStack.IsEmpty()) {
            const i32 nodeIndex = drawStack.Back();
            drawStack.PopBack();
            const u32 depth = depthStack.Back();
            depthStack.PopBack();
            if (nodeIndex < 0 || nodeIndex >= static_cast<i32>(mAssetNodes.Size())) {
                continue;
            }

            auto&                       node = mAssetNodes[static_cast<usize>(nodeIndex)];
            DebugGui::FTreeViewItemDesc itemDesc{};
            itemDesc.mLabel       = node.mName.ToView();
            itemDesc.mDepth       = depth;
            itemDesc.mSelected    = (mCurrentAssetPath == node.mPath);
            itemDesc.mExpanded    = node.mExpanded;
            itemDesc.mHasChildren = !node.mChildren.IsEmpty();

            const auto result = gui.TreeViewItem(itemDesc);
            if (!blockWorkspaceInput && result.mToggleExpanded && !node.mChildren.IsEmpty()) {
                node.mExpanded = !node.mExpanded;
            } else if (!blockWorkspaceInput && result.mClicked) {
                pendingTreeOpenPath = node.mPath;
                bHasPendingTreeOpen = true;
                Core::Logging::LogInfoCategory(
                    TEXT("Editor.UI.Asset"), TEXT("Tree select: {}"), node.mPath.ToView());
            } else if (!blockWorkspaceInput && result.mDoubleClicked && !node.mChildren.IsEmpty()) {
                node.mExpanded = !node.mExpanded;
            }
            if (!blockWorkspaceInput && result.mContextMenuRequested) {
                mAssetContextMenu.mOpen     = true;
                mAssetContextMenu.mItemType = EAssetItemType::Directory;
                mAssetContextMenu.mPath     = node.mPath;
                mAssetContextMenu.mPos      = mouse;
            }

            if (node.mExpanded) {
                for (isize child = static_cast<isize>(node.mChildren.Size()) - 1; child >= 0;
                    --child) {
                    drawStack.PushBack(node.mChildren[static_cast<usize>(child)]);
                    depthStack.PushBack(depth + 1U);
                    if (child == 0) {
                        break;
                    }
                }
            }
        }
        if (!blockWorkspaceInput && bHasPendingTreeOpen) {
            OpenPathInAssetView(pendingTreeOpenPath.ToView(), EAssetItemType::Directory);
        }
        gui.PopClipRect();
        if (treeNeedsScrollBar) {
            const f32 trackH = treeScrollTrackRect.Max.Y() - treeScrollTrackRect.Min.Y();
            const f32 thumbHRaw =
                (treeTotalHeight > 0.0f) ? (treeViewHeight / treeTotalHeight) * trackH : trackH;
            const f32   thumbH         = Clamp(thumbHRaw, thumbMinH, trackH);
            const f32   maxThumbTravel = Core::Math::Max(0.0f, trackH - thumbH);
            const f32   t = (treeMaxScrollY > 0.0f) ? (mAssetTreeScrollY / treeMaxScrollY) : 0.0f;
            const f32   thumbY    = treeScrollTrackRect.Min.Y() + maxThumbTravel * t;
            const FRect thumbRect = MakeRect(
                treeScrollTrackRect.Min.X(), thumbY, treeScrollTrackRect.Max.X(), thumbY + thumbH);
            const bool thumbHovered = IsInside(thumbRect, mouse);
            DrawScrollBar(
                gui, theme, treeScrollTrackRect, thumbRect, thumbHovered, mAssetTreeScrollDragging);
        }

        const f32   itemWidth  = ScalePx(104.0f);
        const f32   itemHeight = ScalePx(92.0f);
        const f32   itemGap    = ScalePx(12.0f);
        const FRect gridContentRect =
            MakeRect(gridRect.Min.X() + ScalePx(14.0f), gridSectionRect.Max.Y() + ScalePx(3.0f),
                gridRect.Max.X() - ScalePx(14.0f), gridRect.Max.Y() - ScalePx(14.0f));
        const f32 gridWidth = gridContentRect.Max.X() - gridContentRect.Min.X();
        i32       columns   = static_cast<i32>(gridWidth / (itemWidth + itemGap));
        if (columns < 1) {
            columns = 1;
        }

        FString        pendingGridOpenPath;
        EAssetItemType pendingGridOpenType = EAssetItemType::None;
        bool           bHasPendingGridOpen = false;

        gui.PushClipRect(gridContentRect);
        for (usize i = 0; i < mAssetItems.Size(); ++i) {
            const i32 col = static_cast<i32>(i % static_cast<usize>(columns));
            const i32 row = static_cast<i32>(i / static_cast<usize>(columns));
            const f32 x0  = gridContentRect.Min.X() + static_cast<f32>(col) * (itemWidth + itemGap);
            const f32 y0 = gridContentRect.Min.Y() + static_cast<f32>(row) * (itemHeight + itemGap);
            const FRect itemRect = MakeRect(x0, y0, x0 + itemWidth, y0 + itemHeight);
            if (itemRect.Max.Y() > gridContentRect.Max.Y()) {
                break;
            }

            auto&                         item = mAssetItems[i];
            DebugGui::FTextedIconViewDesc itemDesc{};
            const auto                    truncatedName =
                TruncateAssetLabel(item.mName.ToView(), itemWidth - ScalePx(12.0f));
            itemDesc.mLabel    = truncatedName.ToView();
            itemDesc.mRect     = itemRect;
            itemDesc.mImageId  = (item.mType == EAssetItemType::Directory) ? mAssetFolderIconImageId
                                                                           : mAssetFileIconImageId;
            itemDesc.mSelected = (mSelectedAssetPath == item.mPath);
            itemDesc.mIsDirectory = (item.mType == EAssetItemType::Directory);

            const auto result = gui.TextedIconView(itemDesc);
            if (!blockWorkspaceInput && result.mClicked) {
                const u64  pathHash      = HashAssetItemPath(item.mPath.ToView());
                const bool isDoubleClick = (mLastAssetClickId == pathHash)
                    && (mStateStore.mFrameCounter - mLastAssetClickFrame <= 24ULL);
                mSelectedAssetPath   = item.mPath;
                mSelectedAssetType   = item.mType;
                mLastAssetClickId    = pathHash;
                mLastAssetClickFrame = mStateStore.mFrameCounter;
                if (isDoubleClick && item.mType == EAssetItemType::Directory) {
                    pendingGridOpenPath = item.mPath;
                    pendingGridOpenType = item.mType;
                    bHasPendingGridOpen = true;
                }
            }
            if (!blockWorkspaceInput && result.mDoubleClicked
                && item.mType == EAssetItemType::Directory) {
                pendingGridOpenPath = item.mPath;
                pendingGridOpenType = item.mType;
                bHasPendingGridOpen = true;
            }
            if (!blockWorkspaceInput && result.mContextMenuRequested) {
                mAssetContextMenu.mOpen     = true;
                mAssetContextMenu.mItemType = item.mType;
                mAssetContextMenu.mPath     = item.mPath;
                mAssetContextMenu.mPos      = mouse;
            }
        }
        gui.PopClipRect();
        if (!blockWorkspaceInput && bHasPendingGridOpen) {
            OpenPathInAssetView(pendingGridOpenPath.ToView(), pendingGridOpenType);
        }

        if (mAssetContextMenu.mOpen && !blockWorkspaceInput) {
            FVector2f  menuPos = mAssetContextMenu.mPos;
            const f32  menuW   = ScalePx(160.0f);
            const f32  rowH    = ScalePx(20.0f);
            const f32  menuH   = rowH * 2.0f + ScalePx(4.0f);
            const auto display = gui.GetDisplaySize();
            if (menuPos.X() + menuW > display.X()) {
                menuPos = FVector2f(display.X() - menuW - ScalePx(2.0f), menuPos.Y());
            }
            if (menuPos.Y() + menuH > display.Y()) {
                menuPos = FVector2f(menuPos.X(), display.Y() - menuH - ScalePx(2.0f));
            }

            const FRect menuRect =
                MakeRect(menuPos.X(), menuPos.Y(), menuPos.X() + menuW, menuPos.Y() + menuH);
            gui.DrawRectFilled(menuRect, colMenuBg);
            gui.DrawRect(menuRect, colBorder, 1.0f);

            const FRect openRect =
                MakeRect(menuRect.Min.X() + ScalePx(2.0f), menuRect.Min.Y() + ScalePx(2.0f),
                    menuRect.Max.X() - ScalePx(2.0f), menuRect.Min.Y() + ScalePx(2.0f) + rowH);
            const FRect refreshRect = MakeRect(menuRect.Min.X() + ScalePx(2.0f), openRect.Max.Y(),
                menuRect.Max.X() - ScalePx(2.0f), openRect.Max.Y() + rowH);

            const bool  openHovered    = IsInside(openRect, mouse);
            const bool  refreshHovered = IsInside(refreshRect, mouse);
            if (openHovered) {
                gui.DrawRectFilled(openRect, colMenuHover);
            }
            if (refreshHovered) {
                gui.DrawRectFilled(refreshRect, colMenuHover);
            }

            const bool canOpen = (mAssetContextMenu.mItemType == EAssetItemType::Directory);
            gui.DrawText(
                FVector2f(openRect.Min.X() + ScalePx(8.0f), openRect.Min.Y() + ScalePx(4.0f)),
                canOpen ? colText : colMutedText, TEXT("Open"));
            gui.DrawText(
                FVector2f(refreshRect.Min.X() + ScalePx(8.0f), refreshRect.Min.Y() + ScalePx(4.0f)),
                colText, TEXT("Refresh"));

            if (gui.WasMousePressed()) {
                if (openHovered && canOpen) {
                    OpenPathInAssetView(
                        mAssetContextMenu.mPath.ToView(), mAssetContextMenu.mItemType);
                    mAssetContextMenu.mOpen = false;
                } else if (refreshHovered) {
                    mAssetNeedsRefresh = true;
                    RefreshAssetCache(true);
                    mAssetContextMenu.mOpen = false;
                } else if (!IsInside(menuRect, mouse)) {
                    mAssetContextMenu.mOpen = false;
                }
            }
        }
    }

} // namespace AltinaEngine::Editor::UI
