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

    struct FEditorGameObjectRuntimeId {
        u32                mWorldId    = 0U;
        u32                mIndex      = 0U;
        u32                mGeneration = 0U;

        [[nodiscard]] auto IsValid() const noexcept -> bool { return mGeneration != 0U; }
        [[nodiscard]] auto operator==(const FEditorGameObjectRuntimeId& rhs) const noexcept
            -> bool {
            return mWorldId == rhs.mWorldId && mIndex == rhs.mIndex
                && mGeneration == rhs.mGeneration;
        }
    };

    struct FEditorGameObjectRuntimeIdHash {
        [[nodiscard]] auto operator()(const FEditorGameObjectRuntimeId& id) const noexcept
            -> usize {
            const u64 a = static_cast<u64>(id.mWorldId);
            const u64 b = static_cast<u64>(id.mIndex);
            const u64 c = static_cast<u64>(id.mGeneration);
            u64       h = a;
            h           = (h ^ (b + 0x9e3779b97f4a7c15ULL + (h << 6U) + (h >> 2U)));
            h           = (h ^ (c + 0x9e3779b97f4a7c15ULL + (h << 6U) + (h >> 2U)));
            return static_cast<usize>(h);
        }
    };

    struct FEditorComponentRuntimeId {
        u64                mType       = 0ULL;
        u32                mIndex      = 0U;
        u32                mGeneration = 0U;

        [[nodiscard]] auto IsValid() const noexcept -> bool {
            return mType != 0ULL && mGeneration != 0U;
        }
        [[nodiscard]] auto operator==(const FEditorComponentRuntimeId& rhs) const noexcept -> bool {
            return mType == rhs.mType && mIndex == rhs.mIndex && mGeneration == rhs.mGeneration;
        }
    };

    enum class EEditorPropertyValueKind : u8 {
        Unknown = 0,
        Scalar,
        Boolean,
        String,
        Array,
        Object
    };

    struct FEditorPropertySnapshot {
        ::AltinaEngine::Core::Container::FString mName;
        ::AltinaEngine::Core::Container::FString mDisplayValue;
        EEditorPropertyValueKind                 mValueKind = EEditorPropertyValueKind::Unknown;
    };

    struct FEditorComponentSnapshot {
        FEditorComponentRuntimeId                                         mId{};
        ::AltinaEngine::Core::Container::FString                          mName;
        ::AltinaEngine::Core::Container::FString                          mTypeName;
        ::AltinaEngine::Core::Container::FString                          mTypeNamespace;
        ::AltinaEngine::Core::Container::TVector<FEditorPropertySnapshot> mProperties;
    };

    struct FEditorGameObjectSnapshot {
        FEditorGameObjectRuntimeId                                         mId{};
        FEditorGameObjectRuntimeId                                         mParentId{};
        ::AltinaEngine::Core::Container::FString                           mName;
        bool                                                               bIsPrefabRoot = false;
        ::AltinaEngine::Core::Container::TVector<FEditorComponentSnapshot> mComponents;
    };

    struct FEditorWorldHierarchySnapshot {
        u32                                                                 mWorldId = 0U;
        ::AltinaEngine::Core::Container::TVector<FEditorGameObjectSnapshot> mGameObjects;
    };

    enum class EEditorSelectionType : u8 {
        None = 0,
        GameObject,
        Component
    };

    struct FEditorSelectionInfo {
        EEditorSelectionType                     mType = EEditorSelectionType::None;
        ::AltinaEngine::Core::Container::FString mName;
        ::AltinaEngine::Core::Container::FString mUuid;
        ::AltinaEngine::Core::Container::FString mTypeName;
        ::AltinaEngine::Core::Container::FString mTypeNamespace;
    };

    struct FEditorHierarchyDebugItem {
        ::AltinaEngine::Core::Container::FString mLabel;
        u32                                      mDepth       = 0U;
        bool                                     mIsComponent = false;
    };

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

    enum class EEditorDockArea : u8 {
        Left = 0,
        Center,
        Right,
        Bottom
    };

    struct FEditorPanelDescriptor {
        ::AltinaEngine::Core::Container::FString mId;
        ::AltinaEngine::Core::Container::FString mTitle;
        EEditorDockArea                          mDockArea = EEditorDockArea::Center;
        bool                                     bVisible  = true;
        i32                                      mPriority = 0;
    };

    struct FEditorUiInitDesc {
        DebugGui::IDebugGuiSystem*                                       mDebugGuiSystem = nullptr;
        ::AltinaEngine::Core::Container::FStringView                     mAssetRoot;
        ::AltinaEngine::Core::Container::FStringView                     mProjectSourcePath;
        ::AltinaEngine::Core::Container::TVector<FEditorPanelDescriptor> mPanels;
    };

    struct FEditorUiFrameContext {
        const FEditorWorldHierarchySnapshot* mHierarchySnapshot  = nullptr;
        bool                                 bClearCommandBuffer = false;
    };

    struct FEditorUiFrameOutput {
        FEditorViewportRequest                                     mViewportRequest{};
        ::AltinaEngine::Core::Container::TVector<EEditorUiCommand> mCommands;
    };

    namespace Testing {
        class FEditorUiTestingAccess;
    }

    class AE_EDITOR_UI_API FEditorUiModule final {
    public:
        void               Initialize(const FEditorUiInitDesc& initDesc);
        [[nodiscard]] auto TickUi(const FEditorUiFrameContext& frameContext)
            -> FEditorUiFrameOutput;
        void               Shutdown();

        [[nodiscard]] auto IsInitialized() const noexcept -> bool { return mRegistered; }

    private:
        friend class Testing::FEditorUiTestingAccess;
        using EDockArea = EEditorDockArea;

        struct FPanelState {
            ::AltinaEngine::Core::Container::FString Name;
            ::AltinaEngine::Core::Container::FString mId;
            EEditorDockArea                          Area      = EEditorDockArea::Center;
            bool                                     bVisible  = true;
            i32                                      mPriority = 0;
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

        struct FEditorUiStateStore {
            FEditorViewportRequest                                     mViewportRequest{};
            ::AltinaEngine::Core::Container::TVector<EEditorUiCommand> mPendingCommands;
            FEditorUiFrameOutput                                       mCachedOutput{};
            u64                                                        mFrameCounter = 0ULL;
        };

        class FEditorUiRootController final {
        public:
            void Draw(FEditorUiModule& module, DebugGui::IDebugGuiSystem* debugGuiSystem,
                DebugGui::IDebugGui& gui) const;
        };

        class FHierarchyPanelController final {
        public:
            void Draw(FEditorUiModule& module, DebugGui::IDebugGui& gui,
                const DebugGui::FRect&                       contentRect,
                const ::AltinaEngine::Core::Math::FVector2f& mouse, bool blockWorkspaceInput) const;
        };

        class FAssetPanelController final {
        public:
            void Draw(FEditorUiModule& module, DebugGui::IDebugGui& gui,
                const DebugGui::FRect&                       contentRect,
                const ::AltinaEngine::Core::Math::FVector2f& mouse, bool blockWorkspaceInput) const;
        };

        class FInspectorPanelController final {
        public:
            void Draw(FEditorUiModule& module, DebugGui::IDebugGui& gui,
                const DebugGui::FRect& contentRect) const;
        };

        void               RegisterDefaultPanels(DebugGui::IDebugGuiSystem* debugGuiSystem,
                          ::AltinaEngine::Core::Container::FStringView      assetRoot         = {},
                          ::AltinaEngine::Core::Container::FStringView      projectSourcePath = {});
        void               SetWorldHierarchySnapshot(const FEditorWorldHierarchySnapshot& snapshot);

        [[nodiscard]] auto GetViewportRequest() const noexcept -> FEditorViewportRequest;
        [[nodiscard]] auto ConsumeUiCommands()
            -> ::AltinaEngine::Core::Container::TVector<EEditorUiCommand>;

        [[nodiscard]] auto DebugGetAssetItemsForTest() const
            -> ::AltinaEngine::Core::Container::TVector<::AltinaEngine::Core::Container::FString>;
        [[nodiscard]] auto DebugGetCurrentAssetPathForTest() const
            -> ::AltinaEngine::Core::Container::FString;
        [[nodiscard]] auto DebugGetHierarchyItemsForTest() const
            -> ::AltinaEngine::Core::Container::TVector<FEditorHierarchyDebugItem>;
        [[nodiscard]] auto DebugGetSelectionInfoForTest() const -> FEditorSelectionInfo;
        [[nodiscard]] auto DebugGetHierarchySnapshotForTest() const
            -> FEditorWorldHierarchySnapshot;
        [[nodiscard]] auto DebugIsInspectorComponentExpandedForTest(
            FEditorComponentRuntimeId id) const -> bool;
        [[nodiscard]] auto DebugGetInspectorScrollYForTest() const -> f32;
        void               DebugSetInspectorScrollYForTest(f32 value);
        void               DebugSelectGameObjectForTest(FEditorGameObjectRuntimeId id);
        void               DebugSelectComponentForTest(FEditorComponentRuntimeId id);
        auto               DebugOpenAssetPathForTest(
                          ::AltinaEngine::Core::Container::FStringView path, EAssetItemType type) -> bool;

        void DrawRootUi(DebugGui::IDebugGuiSystem* debugGuiSystem, DebugGui::IDebugGui& gui);
        void DrawHierarchyPanel(DebugGui::IDebugGui& gui, const DebugGui::FRect& contentRect,
            const ::AltinaEngine::Core::Math::FVector2f& mouse, bool blockWorkspaceInput);
        void DrawInspectorPanel(DebugGui::IDebugGui& gui, const DebugGui::FRect& contentRect);
        void DrawAssetPanel(DebugGui::IDebugGui& gui, const DebugGui::FRect& contentRect,
            const ::AltinaEngine::Core::Math::FVector2f& mouse, bool blockWorkspaceInput);

        void RefreshHierarchyCache();
        void RefreshHierarchyDebugItems();
        void SelectGameObject(FEditorGameObjectRuntimeId id);
        void SelectComponent(FEditorComponentRuntimeId id);
        [[nodiscard]] auto IsGameObjectSelected(FEditorGameObjectRuntimeId id) const -> bool;
        [[nodiscard]] auto IsComponentSelected(FEditorComponentRuntimeId id) const -> bool;
        [[nodiscard]] auto MakeGameObjectUuid(FEditorGameObjectRuntimeId id) const
            -> ::AltinaEngine::Core::Container::FString;
        [[nodiscard]] auto MakeComponentUuid(FEditorComponentRuntimeId id) const
            -> ::AltinaEngine::Core::Container::FString;
        [[nodiscard]] auto FindGameObjectIndex(FEditorGameObjectRuntimeId id) const -> i32;
        [[nodiscard]] auto FindComponentSnapshot(FEditorComponentRuntimeId id) const
            -> const FEditorComponentSnapshot*;
        [[nodiscard]] auto FindSelectedGameObjectSnapshot() const
            -> const FEditorGameObjectSnapshot*;

        void               RefreshAssetCache(bool force);
        void               BuildAssetItemsForCurrentFolder();
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

        DebugGui::IDebugGuiSystem*                            mDebugGuiSystem          = nullptr;
        bool                                                  mRegistered              = false;
        bool                                                  mFocusedViewport         = false;
        i32                                                   mOpenMenu                = -1;
        bool                                                  mOpenMenuUseLegacyAnchor = false;
        i32                                                   mDraggingPanel           = -1;
        bool                                                  mLegacyViewportDragArmed = false;
        i32                                                   mActiveSplitter          = 0;
        FDockState                                            mDock{};
        ::AltinaEngine::Core::Container::TVector<FPanelState> mPanels;
        ::AltinaEngine::Core::Container::TVector<FEditorPanelDescriptor> mPanelDescriptors;

        FEditorUiStateStore                                              mStateStore{};
        FHierarchyPanelController     mHierarchyPanelController{};
        FAssetPanelController         mAssetPanelController{};
        FInspectorPanelController     mInspectorPanelController{};
        FEditorUiRootController       mRootController{};

        FEditorWorldHierarchySnapshot mHierarchySnapshot{};
        ::AltinaEngine::Core::Container::TVector<::AltinaEngine::Core::Container::TVector<i32>>
                                                      mHierarchyChildren;
        ::AltinaEngine::Core::Container::TVector<i32> mHierarchyRoots;
        ::AltinaEngine::Core::Container::THashMap<FEditorGameObjectRuntimeId, i32,
            FEditorGameObjectRuntimeIdHash>
            mHierarchyLookup;
        ::AltinaEngine::Core::Container::THashMap<::AltinaEngine::Core::Container::FString, bool>
                                                                            mHierarchyExpanded;
        ::AltinaEngine::Core::Container::TVector<FEditorHierarchyDebugItem> mHierarchyDebugItems;
        FEditorSelectionInfo                                                mSelection{};
        ::AltinaEngine::Core::Container::FString                            mInspectorNameInput;
        ::AltinaEngine::Core::Container::THashMap<::AltinaEngine::Core::Container::FString, bool>
                                                 mInspectorExpanded;
        f32                                      mInspectorScrollY           = 0.0f;
        f32                                      mInspectorScrollDragOffsetY = 0.0f;
        bool                                     mInspectorScrollDragging    = false;
        f32                                      mHierarchyScrollY           = 0.0f;
        f32                                      mHierarchyScrollDragOffsetY = 0.0f;
        bool                                     mHierarchyScrollDragging    = false;

        ::AltinaEngine::Core::Container::FString mAssetRootPath;
        ::AltinaEngine::Core::Container::FString mCurrentAssetPath;
        ::AltinaEngine::Core::Container::FString mSelectedAssetPath;
        EAssetItemType                           mSelectedAssetType = EAssetItemType::None;
        ::AltinaEngine::Core::Container::TVector<FAssetNode> mAssetNodes;
        ::AltinaEngine::Core::Container::THashMap<::AltinaEngine::Core::Container::FString, i32>
                                                             mAssetNodeLookup;
        ::AltinaEngine::Core::Container::TVector<FAssetItem> mAssetItems;
        FAssetContextMenuState                               mAssetContextMenu;
        u64                                                  mAssetLastRefreshFrame      = 0ULL;
        u64                                                  mLastAssetClickId           = 0ULL;
        u64                                                  mLastAssetClickFrame        = 0ULL;
        f32                                                  mUiScale                    = 1.0f;
        bool                                                 mAssetSplitterActive        = false;
        f32                                                  mAssetTreeSplitRatio        = 0.32f;
        f32                                                  mAssetTreeScrollY           = 0.0f;
        f32                                                  mAssetTreeScrollDragOffsetY = 0.0f;
        bool                                                 mAssetTreeScrollDragging    = false;
        f32                                                  mOutputScrollY              = 0.0f;
        f32                                                  mOutputScrollDragOffsetY    = 0.0f;
        bool                                                 mOutputScrollDragging       = false;
        bool                                                 mAssetNeedsRefresh          = true;
    };
} // namespace AltinaEngine::Editor::UI
