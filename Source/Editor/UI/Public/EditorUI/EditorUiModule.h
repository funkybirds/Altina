#pragma once

#include "Base/EditorUIAPI.h"
#include "Container/HashMap.h"
#include "Container/String.h"
#include "Container/Vector.h"
#include "CoreMinimal.h"
#include "DebugGui/Core/Types.h"
#include "Utility/Filesystem/Path.h"

namespace AltinaEngine::DebugGui {
    class IDebugGui;
    class IDebugGuiSystem;
} // namespace AltinaEngine::DebugGui

namespace AltinaEngine::Editor::UI {
    inline constexpr u64 kEditorViewportImageId = 0xE17D0001ULL;

    enum class EEditorUiCommand : u8 {
        None = 0,
        Play,
        Pause,
        Step,
        Stop,
        Exit
    };

    enum class EAssetItemType : u8 {
        None = 0,
        Directory,
        File
    };

    struct FEditorViewportRequest {
        u32  Width            = 0U;
        u32  Height           = 0U;
        i32  ContentMinX      = 0;
        i32  ContentMinY      = 0;
        bool bHovered         = false;
        bool bFocused         = false;
        bool bHasContent      = false;
        bool bUiBlockingInput = false;
    };

    class AE_EDITOR_UI_API FEditorUiModule final {
    public:
        void               RegisterDefaultPanels(DebugGui::IDebugGuiSystem* debugGuiSystem,
                          ::AltinaEngine::Core::Container::FStringView      assetRoot         = {},
                          ::AltinaEngine::Core::Container::FStringView      projectSourcePath = {});
        [[nodiscard]] auto GetViewportRequest() const noexcept -> FEditorViewportRequest;
        [[nodiscard]] auto ConsumeUiCommands()
            -> ::AltinaEngine::Core::Container::TVector<EEditorUiCommand>;
        [[nodiscard]] auto DebugGetAssetItemsForTest() const
            -> ::AltinaEngine::Core::Container::TVector<::AltinaEngine::Core::Container::FString>;
        [[nodiscard]] auto DebugGetCurrentAssetPathForTest() const
            -> ::AltinaEngine::Core::Container::FString;

    private:
        enum class EDockArea : u8 {
            Left = 0,
            Center,
            Right,
            Bottom
        };

        struct FPanelState {
            ::AltinaEngine::Core::Container::FString Name;
            EDockArea                                Area     = EDockArea::Center;
            bool                                     bVisible = true;
        };

        struct FDockState {
            f32 LeftRatio    = 0.2f;
            f32 RightRatio   = 0.24f;
            f32 BottomRatio  = 0.28f;
            i32 ActiveLeft   = 0;
            i32 ActiveCenter = 0;
            i32 ActiveRight  = 0;
            i32 ActiveBottom = 0;
        };

        struct FAssetNode {
            ::AltinaEngine::Core::Container::FString      mPath;
            ::AltinaEngine::Core::Container::FString      mName;
            i32                                           mParentIndex = -1;
            bool                                          mExpanded    = false;
            bool                                          mVisible     = false;
            ::AltinaEngine::Core::Container::TVector<i32> mChildren;
        };

        struct FAssetItem {
            ::AltinaEngine::Core::Container::FString mPath;
            ::AltinaEngine::Core::Container::FString mName;
            EAssetItemType                           mType       = EAssetItemType::None;
            bool                                     mNavigateUp = false;
        };

        struct FAssetContextMenuState {
            bool                                     mOpen     = false;
            EAssetItemType                           mItemType = EAssetItemType::None;
            ::AltinaEngine::Core::Container::FString mPath;
            ::AltinaEngine::Core::Math::FVector2f    mPos =
                ::AltinaEngine::Core::Math::FVector2f(0.0f, 0.0f);
        };

        void DrawRootUi(DebugGui::IDebugGuiSystem* debugGuiSystem, DebugGui::IDebugGui& gui);
        void DrawAssetPanel(DebugGui::IDebugGui& gui, const DebugGui::FRect& contentRect,
            const ::AltinaEngine::Core::Math::FVector2f& mouse, bool blockWorkspaceInput);
        void RefreshAssetCache(bool force);
        void BuildAssetItemsForCurrentFolder();
        [[nodiscard]] auto ResolveAssetRoot(
            ::AltinaEngine::Core::Container::FStringView requestedRoot) const
            -> ::AltinaEngine::Core::Container::FString;
        [[nodiscard]] auto IsMetaPath(::AltinaEngine::Core::Container::FStringView path) const
            -> bool;
        [[nodiscard]] auto GetAssetDisplayName(
            ::AltinaEngine::Core::Container::FStringView path) const
            -> ::AltinaEngine::Core::Container::FString;
        [[nodiscard]] auto GetNodeIndexByPath(
            ::AltinaEngine::Core::Container::FStringView path) const -> i32;
        [[nodiscard]] auto TruncateAssetLabel(::AltinaEngine::Core::Container::FStringView label,
            f32 maxWidth) const -> ::AltinaEngine::Core::Container::FString;
        void               EnsureNodeVisible(i32 nodeIndex);
        void               OpenPathInAssetView(
                          ::AltinaEngine::Core::Container::FStringView path, EAssetItemType type);

        DebugGui::IDebugGuiSystem*                                 mDebugGuiSystem  = nullptr;
        bool                                                       mRegistered      = false;
        bool                                                       mFocusedViewport = false;
        i32                                                        mOpenMenu        = -1;
        i32                                                        mDraggingPanel   = -1;
        i32                                                        mActiveSplitter  = 0;
        FDockState                                                 mDock{};
        FEditorViewportRequest                                     mViewportRequest{};
        ::AltinaEngine::Core::Container::TVector<FPanelState>      mPanels;
        ::AltinaEngine::Core::Container::TVector<EEditorUiCommand> mPendingCommands;

        ::AltinaEngine::Core::Container::FString                   mAssetRootPath;
        ::AltinaEngine::Core::Container::FString                   mCurrentAssetPath;
        ::AltinaEngine::Core::Container::FString                   mSelectedAssetPath;
        EAssetItemType mSelectedAssetType = EAssetItemType::None;
        ::AltinaEngine::Core::Container::TVector<FAssetNode> mAssetNodes;
        ::AltinaEngine::Core::Container::THashMap<::AltinaEngine::Core::Container::FString, i32>
                                                             mAssetNodeLookup;
        ::AltinaEngine::Core::Container::TVector<FAssetItem> mAssetItems;
        FAssetContextMenuState                               mAssetContextMenu;
        u64                                                  mFrameCounter          = 0ULL;
        u64                                                  mAssetLastRefreshFrame = 0ULL;
        u64                                                  mLastAssetClickId      = 0ULL;
        u64                                                  mLastAssetClickFrame   = 0ULL;
        bool                                                 mAssetSplitterActive   = false;
        f32                                                  mAssetTreeSplitRatio   = 0.32f;
        bool                                                 mAssetNeedsRefresh     = true;
        u64 mAssetFolderIconImageId                                                 = 0xE17D1001ULL;
        u64 mAssetFileIconImageId                                                   = 0xE17D1002ULL;
    };
} // namespace AltinaEngine::Editor::UI
