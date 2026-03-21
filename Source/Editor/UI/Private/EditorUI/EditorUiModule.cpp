#include "EditorUI/EditorUiModule.h"

#include "DebugGui/DebugGui.h"
#include "Math/Common.h"
#include "Math/Vector.h"
#include "Utility/EngineConfig/EngineConfig.h"
#include "Utility/Filesystem/FileSystem.h"

namespace AltinaEngine::Editor::UI {
    namespace {
        using ::AltinaEngine::Core::Container::FString;
        using ::AltinaEngine::Core::Container::FStringView;
        using ::AltinaEngine::Core::Container::TVector;
        using Core::Math::FVector2f;
        using Core::Utility::Filesystem::FPath;
    } // namespace

    void FEditorUiModule::Initialize(const FEditorUiInitDesc& initDesc) {
        mPanelDescriptors = initDesc.mPanels;
        RegisterDefaultPanels(
            initDesc.mDebugGuiSystem, initDesc.mAssetRoot, initDesc.mProjectSourcePath);
    }

    auto FEditorUiModule::TickUi(const FEditorUiFrameContext& frameContext)
        -> FEditorUiFrameOutput {
        if (frameContext.mHierarchySnapshot != nullptr) {
            SetWorldHierarchySnapshot(*frameContext.mHierarchySnapshot);
        }
        mPlayState = frameContext.mPlayState;

        mStateStore.mCachedOutput.mViewportRequest = GetViewportRequest();
        mStateStore.mCachedOutput.mCommands        = mStateStore.mPendingCommands;

        if (frameContext.bClearCommandBuffer) {
            mStateStore.mPendingCommands.Clear();
        }

        return mStateStore.mCachedOutput;
    }

    void FEditorUiModule::Shutdown() {
        mRegistered     = false;
        mDebugGuiSystem = nullptr;
        mPanels.Clear();
        mPanelDescriptors.Clear();
        mStateStore = {};
    }

    void FEditorUiModule::FEditorUiRootController::Draw(FEditorUiModule& module,
        DebugGui::IDebugGuiSystem* debugGuiSystem, DebugGui::IDebugGui& gui) const {
        module.DrawRootUi(debugGuiSystem, gui);
    }

    void FEditorUiModule::FHierarchyPanelController::Draw(FEditorUiModule& module,
        DebugGui::IDebugGui& gui, const DebugGui::FRect& contentRect,
        const ::AltinaEngine::Core::Math::FVector2f& mouse, bool blockWorkspaceInput) const {
        module.DrawHierarchyPanel(gui, contentRect, mouse, blockWorkspaceInput);
    }

    void FEditorUiModule::FAssetPanelController::Draw(FEditorUiModule& module,
        DebugGui::IDebugGui& gui, const DebugGui::FRect& contentRect,
        const ::AltinaEngine::Core::Math::FVector2f& mouse, bool blockWorkspaceInput) const {
        module.DrawAssetPanel(gui, contentRect, mouse, blockWorkspaceInput);
    }

    void FEditorUiModule::FInspectorPanelController::Draw(FEditorUiModule& module,
        DebugGui::IDebugGui& gui, const DebugGui::FRect& contentRect) const {
        module.DrawInspectorPanel(gui, contentRect);
    }

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

        if (mPanelDescriptors.IsEmpty()) {
            mPanelDescriptors.PushBack({ FString(TEXT("hierarchy")), FString(TEXT("Hierarchy")),
                EEditorDockArea::Left, true, 0 });
            mPanelDescriptors.PushBack({ FString(TEXT("viewport")), FString(TEXT("Viewport")),
                EEditorDockArea::Center, true, 1 });
            mPanelDescriptors.PushBack({ FString(TEXT("inspector")), FString(TEXT("Inspector")),
                EEditorDockArea::Right, true, 2 });
            mPanelDescriptors.PushBack({ FString(TEXT("asset")), FString(TEXT("Asset")),
                EEditorDockArea::Bottom, true, 3 });
            mPanelDescriptors.PushBack({ FString(TEXT("output")), FString(TEXT("Output")),
                EEditorDockArea::Bottom, true, 4 });
        }

        mPanels.Clear();
        for (const auto& descriptor : mPanelDescriptors) {
            FPanelState state{};
            state.Name      = descriptor.mTitle;
            state.mId       = descriptor.mId;
            state.Area      = descriptor.mDockArea;
            state.bVisible  = descriptor.bVisible;
            state.mPriority = descriptor.mPriority;
            mPanels.PushBack(Move(state));
        }

        mDock.ActiveLeft   = 0;
        mDock.ActiveCenter = 1;
        mDock.ActiveRight  = 2;
        mDock.ActiveBottom = 3;

        debugGuiSystem->RegisterBackgroundOverlay(
            TEXT("Editor.UI.Root"), [this, debugGuiSystem](DebugGui::IDebugGui& gui) {
                mRootController.Draw(*this, debugGuiSystem, gui);
            });
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
        return mStateStore.mViewportRequest;
    }

    auto FEditorUiModule::ConsumeUiCommands()
        -> ::AltinaEngine::Core::Container::TVector<EEditorUiCommand> {
        auto out = mStateStore.mPendingCommands;
        mStateStore.mPendingCommands.Clear();
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

    void FEditorUiModule::SetWorldHierarchySnapshot(const FEditorWorldHierarchySnapshot& snapshot) {
        mHierarchySnapshot = snapshot;
        RefreshHierarchyCache();
        RefreshHierarchyDebugItems();
    }

    auto FEditorUiModule::DebugGetHierarchyItemsForTest() const
        -> ::AltinaEngine::Core::Container::TVector<FEditorHierarchyDebugItem> {
        return mHierarchyDebugItems;
    }

    auto FEditorUiModule::DebugGetSelectionInfoForTest() const -> FEditorSelectionInfo {
        return mSelection;
    }

    auto FEditorUiModule::DebugGetHierarchySnapshotForTest() const
        -> FEditorWorldHierarchySnapshot {
        return mHierarchySnapshot;
    }

    auto FEditorUiModule::DebugIsInspectorComponentExpandedForTest(
        FEditorComponentRuntimeId id) const -> bool {
        auto it = mInspectorExpanded.FindIt(MakeComponentUuid(id));
        if (it == mInspectorExpanded.end()) {
            return false;
        }
        return it->second;
    }

    auto FEditorUiModule::DebugGetInspectorScrollYForTest() const -> f32 {
        return mInspectorScrollY;
    }

    void FEditorUiModule::DebugSetInspectorScrollYForTest(f32 value) { mInspectorScrollY = value; }

    void FEditorUiModule::DebugSelectGameObjectForTest(FEditorGameObjectRuntimeId id) {
        SelectGameObject(id);
    }

    void FEditorUiModule::DebugSelectComponentForTest(FEditorComponentRuntimeId id) {
        SelectComponent(id);
    }

    auto FEditorUiModule::DebugOpenAssetPathForTest(
        Core::Container::FStringView path, EAssetItemType type) -> bool {
        if (path.IsEmpty()) {
            return false;
        }
        OpenPathInAssetView(path, type);
        return true;
    }
} // namespace AltinaEngine::Editor::UI
