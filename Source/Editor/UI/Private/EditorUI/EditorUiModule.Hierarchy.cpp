#include "EditorUI/EditorUiModule.h"

#include "Algorithm/Sort.h"

namespace AltinaEngine::Editor::UI {
    namespace {
        using ::AltinaEngine::Core::Container::TVector;
    }

    auto FEditorUiModule::MakeGameObjectUuid(FEditorGameObjectRuntimeId id) const
        -> Core::Container::FString {
        Core::Container::FString uuid;
        uuid.AppendNumber(id.mWorldId);
        uuid.Append(TEXT("-"));
        uuid.AppendNumber(id.mIndex);
        uuid.Append(TEXT("-"));
        uuid.AppendNumber(id.mGeneration);
        return uuid;
    }

    auto FEditorUiModule::MakeComponentUuid(FEditorComponentRuntimeId id) const
        -> Core::Container::FString {
        Core::Container::FString uuid;
        uuid.AppendNumber(id.mType);
        uuid.Append(TEXT("-"));
        uuid.AppendNumber(id.mIndex);
        uuid.Append(TEXT("-"));
        uuid.AppendNumber(id.mGeneration);
        return uuid;
    }

    auto FEditorUiModule::FindGameObjectIndex(FEditorGameObjectRuntimeId id) const -> i32 {
        auto it = mHierarchyLookup.FindIt(id);
        if (it == mHierarchyLookup.end()) {
            return -1;
        }
        return it->second;
    }

    auto FEditorUiModule::FindComponentSnapshot(FEditorComponentRuntimeId id) const
        -> const FEditorComponentSnapshot* {
        for (const auto& object : mHierarchySnapshot.mGameObjects) {
            for (const auto& component : object.mComponents) {
                if (component.mId == id) {
                    return &component;
                }
            }
        }
        return nullptr;
    }

    auto FEditorUiModule::FindSelectedGameObjectSnapshot() const
        -> const FEditorGameObjectSnapshot* {
        if (mSelection.mType != EEditorSelectionType::GameObject) {
            return nullptr;
        }
        for (const auto& object : mHierarchySnapshot.mGameObjects) {
            if (mSelection.mUuid == MakeGameObjectUuid(object.mId).ToView()) {
                return &object;
            }
        }
        return nullptr;
    }

    auto FEditorUiModule::IsGameObjectSelected(FEditorGameObjectRuntimeId id) const -> bool {
        if (mSelection.mType != EEditorSelectionType::GameObject) {
            return false;
        }
        return mSelection.mUuid == MakeGameObjectUuid(id).ToView();
    }

    auto FEditorUiModule::IsComponentSelected(FEditorComponentRuntimeId id) const -> bool {
        if (mSelection.mType != EEditorSelectionType::Component) {
            return false;
        }
        return mSelection.mUuid == MakeComponentUuid(id).ToView();
    }

    void FEditorUiModule::SelectGameObject(FEditorGameObjectRuntimeId id) {
        const i32 index = FindGameObjectIndex(id);
        if (index < 0 || index >= static_cast<i32>(mHierarchySnapshot.mGameObjects.Size())) {
            mSelection = {};
            mInspectorNameInput.Clear();
            return;
        }

        const auto& object = mHierarchySnapshot.mGameObjects[static_cast<usize>(index)];
        mSelection.mType   = EEditorSelectionType::GameObject;
        mSelection.mName   = object.mName;
        mSelection.mUuid   = MakeGameObjectUuid(object.mId);
        mSelection.mTypeName.Clear();
        mSelection.mTypeNamespace.Clear();
        mInspectorNameInput = object.mName;
    }

    void FEditorUiModule::SelectComponent(FEditorComponentRuntimeId id) {
        const auto* component = FindComponentSnapshot(id);
        if (component == nullptr) {
            mSelection = {};
            mInspectorNameInput.Clear();
            return;
        }

        mSelection.mType          = EEditorSelectionType::Component;
        mSelection.mName          = component->mName;
        mSelection.mUuid          = MakeComponentUuid(component->mId);
        mSelection.mTypeName      = component->mTypeName;
        mSelection.mTypeNamespace = component->mTypeNamespace;
        mInspectorNameInput.Clear();
    }

    void FEditorUiModule::RefreshHierarchyCache() {
        mHierarchyChildren.Clear();
        mHierarchyRoots.Clear();
        mHierarchyLookup.Clear();

        mHierarchyChildren.Resize(mHierarchySnapshot.mGameObjects.Size());
        for (usize i = 0; i < mHierarchySnapshot.mGameObjects.Size(); ++i) {
            mHierarchyLookup[mHierarchySnapshot.mGameObjects[i].mId] = static_cast<i32>(i);
        }

        const auto findParentIndex = [this](const FEditorGameObjectRuntimeId& parentId) -> i32 {
            const i32 directIndex = FindGameObjectIndex(parentId);
            if (directIndex >= 0) {
                return directIndex;
            }
            if (parentId.mWorldId == 0U && parentId.mIndex == 0U && parentId.mGeneration == 0U) {
                return -1;
            }
            for (i32 index = 0; index < static_cast<i32>(mHierarchySnapshot.mGameObjects.Size());
                ++index) {
                const auto& candidate =
                    mHierarchySnapshot.mGameObjects[static_cast<usize>(index)].mId;
                if (candidate.mIndex != parentId.mIndex) {
                    continue;
                }
                if (parentId.mWorldId != 0U && candidate.mWorldId != parentId.mWorldId) {
                    continue;
                }
                return index;
            }
            return -1;
        };

        for (i32 i = 0; i < static_cast<i32>(mHierarchySnapshot.mGameObjects.Size()); ++i) {
            const auto& object      = mHierarchySnapshot.mGameObjects[static_cast<usize>(i)];
            const i32   parentIndex = findParentIndex(object.mParentId);
            const auto  key         = MakeGameObjectUuid(object.mId);
            auto        it          = mHierarchyExpanded.FindIt(key);
            const bool  isNewNode   = (it == mHierarchyExpanded.end());
            if (isNewNode) {
                mHierarchyExpanded[Move(key)] = false;
            }
            if (parentIndex >= 0
                && parentIndex < static_cast<i32>(mHierarchySnapshot.mGameObjects.Size())) {
                mHierarchyChildren[static_cast<usize>(parentIndex)].PushBack(i);
            } else {
                mHierarchyRoots.PushBack(i);
            }
        }

        for (auto& children : mHierarchyChildren) {
            if (children.IsEmpty()) {
                continue;
            }
            TVector<i32> filtered;
            filtered.Reserve(children.Size());
            for (const i32 childIndex : children) {
                if (childIndex >= 0
                    && childIndex < static_cast<i32>(mHierarchySnapshot.mGameObjects.Size())) {
                    filtered.PushBack(childIndex);
                }
            }
            children = Move(filtered);
        }

        Core::Algorithm::Sort(mHierarchyRoots, [this](i32 lhs, i32 rhs) {
            const auto& lhsObject = mHierarchySnapshot.mGameObjects[static_cast<usize>(lhs)];
            const auto& rhsObject = mHierarchySnapshot.mGameObjects[static_cast<usize>(rhs)];
            if (lhsObject.mId.mIndex != rhsObject.mId.mIndex) {
                return lhsObject.mId.mIndex < rhsObject.mId.mIndex;
            }
            return lhsObject.mId.mGeneration < rhsObject.mId.mGeneration;
        });

        for (auto& children : mHierarchyChildren) {
            Core::Algorithm::Sort(children, [this](i32 lhs, i32 rhs) {
                const auto& lhsObject = mHierarchySnapshot.mGameObjects[static_cast<usize>(lhs)];
                const auto& rhsObject = mHierarchySnapshot.mGameObjects[static_cast<usize>(rhs)];
                if (lhsObject.mId.mIndex != rhsObject.mId.mIndex) {
                    return lhsObject.mId.mIndex < rhsObject.mId.mIndex;
                }
                return lhsObject.mId.mGeneration < rhsObject.mId.mGeneration;
            });
        }

        bool selectionValid = false;
        if (mSelection.mType == EEditorSelectionType::GameObject) {
            for (const auto& object : mHierarchySnapshot.mGameObjects) {
                if (mSelection.mUuid == MakeGameObjectUuid(object.mId).ToView()) {
                    selectionValid = true;
                    break;
                }
            }
        }

        if (!selectionValid) {
            mSelection = {};
            mInspectorNameInput.Clear();
        } else if (mSelection.mType == EEditorSelectionType::GameObject) {
            const auto* object = FindSelectedGameObjectSnapshot();
            if (object != nullptr) {
                mSelection.mName    = object->mName;
                mInspectorNameInput = object->mName;
            }
        } else {
            mSelection = {};
            mInspectorNameInput.Clear();
        }
    }

    void FEditorUiModule::RefreshHierarchyDebugItems() {
        mHierarchyDebugItems.Clear();
        TVector<i32> stack;
        for (isize i = static_cast<isize>(mHierarchyRoots.Size()) - 1; i >= 0; --i) {
            stack.PushBack(mHierarchyRoots[static_cast<usize>(i)]);
            if (i == 0) {
                break;
            }
        }

        TVector<u32> depthStack;
        for (usize i = 0; i < stack.Size(); ++i) {
            depthStack.PushBack(0U);
        }

        while (!stack.IsEmpty()) {
            const i32 objectIndex = stack.Back();
            stack.PopBack();
            const u32 depth = depthStack.Back();
            depthStack.PopBack();

            if (objectIndex < 0
                || objectIndex >= static_cast<i32>(mHierarchySnapshot.mGameObjects.Size())) {
                continue;
            }

            const auto& object = mHierarchySnapshot.mGameObjects[static_cast<usize>(objectIndex)];
            FEditorHierarchyDebugItem objectItem{};
            objectItem.mLabel       = object.mName;
            objectItem.mDepth       = depth;
            objectItem.mIsComponent = false;
            mHierarchyDebugItems.PushBack(Move(objectItem));

            const auto& children = mHierarchyChildren[static_cast<usize>(objectIndex)];
            for (isize child = static_cast<isize>(children.Size()) - 1; child >= 0; --child) {
                stack.PushBack(children[static_cast<usize>(child)]);
                depthStack.PushBack(depth + 1U);
                if (child == 0) {
                    break;
                }
            }
        }
    }

} // namespace AltinaEngine::Editor::UI
