#include "EditorUI/EditorUiModule.h"

#include "Algorithm/Sort.h"
#include "DebugGui/DebugGui.h"
#include "Math/Common.h"
#include "Math/Vector.h"
#include "Logging/Log.h"
#include "Utility/EngineConfig/EngineConfig.h"
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

        constexpr f32      kMenuBarHeight              = 24.0f;
        constexpr f32      kWorkspacePad               = 4.0f;
        constexpr f32      kSplitterSize               = 4.0f;
        constexpr f32      kTabBarHeight               = 22.0f;
        constexpr f32      kPanelPadding               = 6.0f;
        constexpr f32      kMinPanelWidth              = 140.0f;
        constexpr f32      kMinCenterWidth             = 260.0f;
        constexpr f32      kMinTopHeight               = 180.0f;
        constexpr f32      kMinBottomHeight            = 100.0f;
        constexpr f32      kGlyphW                     = 8.0f;
        constexpr u64      kAssetRefreshIntervalFrames = 60ULL;

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

    void FEditorUiModule::RegisterDefaultPanels(DebugGui::IDebugGuiSystem* debugGuiSystem,
        Core::Container::FStringView assetRoot, Core::Container::FStringView projectSourcePath) {
        if (debugGuiSystem == nullptr) {
            return;
        }

        mDebugGuiSystem = debugGuiSystem;
        if (mRegistered) {
            return;
        }
        mRegistered = true;

        mAssetRootPath = ResolveAssetRoot(assetRoot);
        if (mAssetRootPath.IsEmptyString() && !projectSourcePath.IsEmpty()) {
            FPath       projectFilePath(projectSourcePath);
            const FPath projectConfigDir = projectFilePath.ParentPath().Normalized();
            FPath       projectRoot      = projectConfigDir;
            if (projectConfigDir.Filename() == FStringView(TEXT("Config"))) {
                projectRoot = projectConfigDir.ParentPath().Normalized();
            }
            FPath projectAssetRoot = (projectRoot / TEXT("Assets")).Normalized();
            if (Core::Platform::IsPathExist(projectAssetRoot.GetString())) {
                mAssetRootPath = projectAssetRoot.GetString();
            }
        }
        mCurrentAssetPath  = mAssetRootPath;
        mAssetNeedsRefresh = true;

        mPanels.Clear();
        mPanels.PushBack({ FString(TEXT("Hierarchy")), EDockArea::Left, true });
        mPanels.PushBack({ FString(TEXT("Viewport")), EDockArea::Center, true });
        mPanels.PushBack({ FString(TEXT("Inspector")), EDockArea::Right, true });
        mPanels.PushBack({ FString(TEXT("Asset")), EDockArea::Bottom, true });
        mPanels.PushBack({ FString(TEXT("Output")), EDockArea::Bottom, true });

        mDock.ActiveLeft   = 0;
        mDock.ActiveCenter = 1;
        mDock.ActiveRight  = 2;
        mDock.ActiveBottom = 3;

        mDebugGuiSystem->SetImageTexture(mAssetFolderIconImageId, nullptr);
        mDebugGuiSystem->SetImageTexture(mAssetFileIconImageId, nullptr);

        debugGuiSystem->RegisterBackgroundOverlay(TEXT("Editor.UI.Root"),
            [this, debugGuiSystem](DebugGui::IDebugGui& gui) { DrawRootUi(debugGuiSystem, gui); });
    }

    auto FEditorUiModule::ResolveAssetRoot(Core::Container::FStringView requestedRoot) const
        -> Core::Container::FString {
        if (!requestedRoot.IsEmpty()) {
            const FPath requested       = FPath(requestedRoot).Normalized();
            const auto  requestedString = requested.GetString();
            if (Core::Platform::IsPathExist(requestedString)) {
                const auto parent = requested.ParentPath().Normalized();
                if (requested.Filename() == FStringView(TEXT("Assets"))
                    && parent.Filename() == FStringView(TEXT("Binaries"))) {
                    const FPath sourceAssets =
                        (parent.ParentPath().Normalized() / TEXT("Assets")).Normalized();
                    const auto sourceAssetsString = sourceAssets.GetString();
                    if (Core::Platform::IsPathExist(sourceAssetsString)) {
                        return sourceAssetsString;
                    }
                }
                return requestedString;
            }
        }

        const auto configured =
            Core::Utility::EngineConfig::GetGlobalConfig().GetString(TEXT("GameClient/AssetRoot"));
        if (!configured.IsEmptyString()) {
            FPath configuredPath(configured);
            if (configuredPath.IsAbsolute()) {
                return configuredPath.Normalized().GetString();
            }
            FPath cwd(Core::Platform::GetCurrentWorkingDir());
            cwd /= configured;
            return cwd.Normalized().GetString();
        }

        FPath cwd(Core::Platform::GetCurrentWorkingDir());
        cwd /= TEXT("Assets");
        return cwd.Normalized().GetString();
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

    auto FEditorUiModule::DebugGetAssetItemsForTest() const
        -> ::AltinaEngine::Core::Container::TVector<::AltinaEngine::Core::Container::FString> {
        ::AltinaEngine::Core::Container::TVector<::AltinaEngine::Core::Container::FString> out;
        out.Reserve(mAssetItems.Size());
        for (const auto& item : mAssetItems) {
            out.PushBack(item.mPath);
        }
        return out;
    }

    auto FEditorUiModule::DebugGetCurrentAssetPathForTest() const
        -> ::AltinaEngine::Core::Container::FString {
        return mCurrentAssetPath;
    }

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

        const i32 maxChars = static_cast<i32>(maxWidth / kGlyphW);
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
            && (mFrameCounter - mAssetLastRefreshFrame) < kAssetRefreshIntervalFrames) {
            return;
        }

        mAssetNeedsRefresh     = false;
        mAssetLastRefreshFrame = mFrameCounter;

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

                FAssetNode node{};
                node.mPath        = dirPath;
                node.mName        = GetAssetDisplayName(dirPath.ToView());
                node.mParentIndex = parentIt->second;
                auto expandedIt   = oldExpanded.FindIt(node.mPath);
                node.mExpanded    = (expandedIt != oldExpanded.end()) ? expandedIt->second : false;

                const i32 index              = static_cast<i32>(mAssetNodes.Size());
                mAssetNodeLookup[node.mPath] = index;
                mAssetNodes.PushBack(Move(node));
                mAssetNodes[static_cast<usize>(parentIt->second)].mChildren.PushBack(index);
            }

            for (auto& node : mAssetNodes) {
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

    void FEditorUiModule::DrawAssetPanel(DebugGui::IDebugGui& gui,
        const DebugGui::FRect& contentRect, const Core::Math::FVector2f& mouse,
        bool blockWorkspaceInput) {
        const auto colPanelBg        = DebugGui::MakeColor32(26, 29, 35, 255);
        const auto colBorder         = DebugGui::MakeColor32(88, 94, 108, 255);
        const auto colText           = DebugGui::MakeColor32(224, 226, 230, 255);
        const auto colMutedText      = DebugGui::MakeColor32(160, 168, 180, 255);
        const auto colMenuBg         = DebugGui::MakeColor32(28, 31, 36, 250);
        const auto colMenuHover      = DebugGui::MakeColor32(48, 54, 63, 255);
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

        const f32 panelWidth    = contentRect.Max.X() - contentRect.Min.X();
        const f32 minTreeWidth  = 120.0f;
        const f32 minGridWidth  = 180.0f;
        const f32 splitterWidth = 6.0f;
        const f32 minSplitX     = contentRect.Min.X() + minTreeWidth;
        const f32 maxSplitRaw   = contentRect.Max.X() - minGridWidth - splitterWidth;
        const f32 maxSplitX     = (maxSplitRaw > minSplitX) ? maxSplitRaw : minSplitX;
        const f32 splitX =
            Clamp(contentRect.Min.X() + panelWidth * mAssetTreeSplitRatio, minSplitX, maxSplitX);
        const FRect treeRect =
            MakeRect(contentRect.Min.X(), contentRect.Min.Y(), splitX, contentRect.Max.Y());
        const FRect splitterRect =
            MakeRect(splitX, contentRect.Min.Y(), splitX + splitterWidth, contentRect.Max.Y());
        const FRect gridRect = MakeRect(
            splitterRect.Max.X(), contentRect.Min.Y(), contentRect.Max.X(), contentRect.Max.Y());

        if (!blockWorkspaceInput && gui.WasMousePressed() && IsInside(splitterRect, mouse)) {
            mAssetSplitterActive = true;
        }
        if (!gui.IsMouseDown()) {
            mAssetSplitterActive = false;
        }
        if (!blockWorkspaceInput && mAssetSplitterActive && gui.IsMouseDown()) {
            const f32 newSplit   = Clamp(mouse.X() - splitterWidth * 0.5f, minSplitX, maxSplitX);
            mAssetTreeSplitRatio = (newSplit - contentRect.Min.X()) / panelWidth;
        }

        gui.DrawRectFilled(treeRect, colPanelBg);
        gui.DrawRect(treeRect, colBorder, 1.0f);
        gui.DrawRectFilled(splitterRect,
            (mAssetSplitterActive || IsInside(splitterRect, mouse)) ? colMenuHover : colBorder);
        gui.DrawRectFilled(gridRect, colPanelBg);
        gui.DrawRect(gridRect, colBorder, 1.0f);

        gui.PushClipRect(treeRect);
        gui.SetCursorPos(FVector2f(treeRect.Min.X() + 4.0f, treeRect.Min.Y() + 2.0f));

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

        const f32 itemWidth  = 94.0f;
        const f32 itemHeight = 84.0f;
        const f32 itemGap    = 8.0f;
        const f32 gridWidth  = gridRect.Max.X() - gridRect.Min.X() - 8.0f;
        i32       columns    = static_cast<i32>(gridWidth / (itemWidth + itemGap));
        if (columns < 1) {
            columns = 1;
        }

        gui.PushClipRect(gridRect);
        for (usize i = 0; i < mAssetItems.Size(); ++i) {
            const i32 col = static_cast<i32>(i % static_cast<usize>(columns));
            const i32 row = static_cast<i32>(i / static_cast<usize>(columns));
            const f32 x0  = gridRect.Min.X() + 4.0f + static_cast<f32>(col) * (itemWidth + itemGap);
            const f32 y0 = gridRect.Min.Y() + 4.0f + static_cast<f32>(row) * (itemHeight + itemGap);
            const FRect itemRect = MakeRect(x0, y0, x0 + itemWidth, y0 + itemHeight);
            if (itemRect.Max.Y() > gridRect.Max.Y()) {
                break;
            }

            auto&                         item = mAssetItems[i];
            DebugGui::FTextedIconViewDesc itemDesc{};
            const auto truncatedName = TruncateAssetLabel(item.mName.ToView(), itemWidth - 12.0f);
            itemDesc.mLabel          = truncatedName.ToView();
            itemDesc.mRect           = itemRect;
            itemDesc.mImageId  = (item.mType == EAssetItemType::Directory) ? mAssetFolderIconImageId
                                                                           : mAssetFileIconImageId;
            itemDesc.mSelected = (mSelectedAssetPath == item.mPath);
            itemDesc.mIsDirectory = (item.mType == EAssetItemType::Directory);

            const auto result = gui.TextedIconView(itemDesc);
            if (!blockWorkspaceInput && result.mClicked) {
                const u64  pathHash      = HashAssetItemPath(item.mPath.ToView());
                const bool isDoubleClick = (mLastAssetClickId == pathHash)
                    && (mFrameCounter - mLastAssetClickFrame <= 24ULL);
                mSelectedAssetPath   = item.mPath;
                mSelectedAssetType   = item.mType;
                mLastAssetClickId    = pathHash;
                mLastAssetClickFrame = mFrameCounter;
                if (isDoubleClick && item.mType == EAssetItemType::Directory) {
                    OpenPathInAssetView(item.mPath.ToView(), item.mType);
                }
            }
            if (!blockWorkspaceInput && result.mDoubleClicked
                && item.mType == EAssetItemType::Directory) {
                OpenPathInAssetView(item.mPath.ToView(), item.mType);
            }
            if (!blockWorkspaceInput && result.mContextMenuRequested) {
                mAssetContextMenu.mOpen     = true;
                mAssetContextMenu.mItemType = item.mType;
                mAssetContextMenu.mPath     = item.mPath;
                mAssetContextMenu.mPos      = mouse;
            }
        }
        gui.PopClipRect();

        if (mAssetContextMenu.mOpen && !blockWorkspaceInput) {
            FVector2f  menuPos = mAssetContextMenu.mPos;
            const f32  menuW   = 160.0f;
            const f32  rowH    = 20.0f;
            const f32  menuH   = rowH * 2.0f + 4.0f;
            const auto display = gui.GetDisplaySize();
            if (menuPos.X() + menuW > display.X()) {
                menuPos = FVector2f(display.X() - menuW - 2.0f, menuPos.Y());
            }
            if (menuPos.Y() + menuH > display.Y()) {
                menuPos = FVector2f(menuPos.X(), display.Y() - menuH - 2.0f);
            }

            const FRect menuRect =
                MakeRect(menuPos.X(), menuPos.Y(), menuPos.X() + menuW, menuPos.Y() + menuH);
            gui.DrawRectFilled(menuRect, colMenuBg);
            gui.DrawRect(menuRect, colBorder, 1.0f);

            const FRect openRect    = MakeRect(menuRect.Min.X() + 2.0f, menuRect.Min.Y() + 2.0f,
                   menuRect.Max.X() - 2.0f, menuRect.Min.Y() + 2.0f + rowH);
            const FRect refreshRect = MakeRect(menuRect.Min.X() + 2.0f, openRect.Max.Y(),
                menuRect.Max.X() - 2.0f, openRect.Max.Y() + rowH);

            const bool  openHovered    = IsInside(openRect, mouse);
            const bool  refreshHovered = IsInside(refreshRect, mouse);
            if (openHovered) {
                gui.DrawRectFilled(openRect, colMenuHover);
            }
            if (refreshHovered) {
                gui.DrawRectFilled(refreshRect, colMenuHover);
            }

            const bool canOpen = (mAssetContextMenu.mItemType == EAssetItemType::Directory);
            gui.DrawText(FVector2f(openRect.Min.X() + 8.0f, openRect.Min.Y() + 4.0f),
                canOpen ? colText : colMutedText, TEXT("Open"));
            gui.DrawText(FVector2f(refreshRect.Min.X() + 8.0f, refreshRect.Min.Y() + 4.0f), colText,
                TEXT("Refresh"));

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

    void FEditorUiModule::DrawRootUi(
        DebugGui::IDebugGuiSystem* debugGuiSystem, DebugGui::IDebugGui& gui) {
        ++mFrameCounter;
        mViewportRequest = {};
        RefreshAssetCache(false);

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

        const bool blockWorkspaceInput    = (mOpenMenu >= 0 && mOpenMenu < 3);
        mViewportRequest.bUiBlockingInput = blockWorkspaceInput || mAssetContextMenu.mOpen;

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
                mViewportRequest.ContentMinX = static_cast<i32>(contentRect.Min.X());
                mViewportRequest.ContentMinY = static_cast<i32>(contentRect.Min.Y());
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

            if (panel.Name.ToView() == FStringView(TEXT("Asset"))) {
                DrawAssetPanel(gui, contentRect, mouse, blockWorkspaceInput);
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
            const usize itemCount = (mOpenMenu == 1) ? 8U : 5U;
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
