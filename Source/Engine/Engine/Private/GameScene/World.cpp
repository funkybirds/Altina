#include "Engine/GameScene/World.h"

#include "Engine/GameScene/CameraComponent.h"
#include "Engine/GameScene/MeshMaterialComponent.h"
#include "Engine/GameScene/StaticMeshFilterComponent.h"

using AltinaEngine::Move;
using AltinaEngine::Core::Container::FStringView;
namespace AltinaEngine::GameScene {
    namespace {
        Core::Threading::TAtomic<u32> gNextWorldId(1);

        auto                          AcquireWorldId() -> u32 { return gNextWorldId.FetchAdd(1); }

        const FComponentTypeHash      kCameraComponentType =
            GetComponentTypeHash<FCameraComponent>();
        const FComponentTypeHash      kStaticMeshComponentType =
            GetComponentTypeHash<FStaticMeshFilterComponent>();
        const FComponentTypeHash      kMeshMaterialComponentType =
            GetComponentTypeHash<FMeshMaterialComponent>();
    } // namespace

    FWorld::FWorld() : mWorldId(AcquireWorldId()) {}

    FWorld::FWorld(u32 worldId) : mWorldId(worldId == 0 ? AcquireWorldId() : worldId) {}

    FWorld::~FWorld() {
        for (u32 index = 0; index < static_cast<u32>(mGameObjects.Size()); ++index) {
            if (!mGameObjects[index].Alive) {
                continue;
            }
            FGameObjectId id{};
            id.Index      = index;
            id.Generation = mGameObjects[index].Generation;
            id.WorldId    = mWorldId;
            DestroyGameObject(id);
        }

        for (auto& entry : mComponentStorage) {
            if (entry.second) {
                entry.second->DestroyAll(*this);
            }
        }

        mComponentStorage.clear();
        mGameObjects.Clear();
        mFreeGameObjects.Clear();
    }

    auto FWorld::CreateGameObject(FStringView name) -> FGameObjectId {
        u32 index = 0;
        if (!mFreeGameObjects.IsEmpty()) {
            index = mFreeGameObjects.Back();
            mFreeGameObjects.PopBack();
        } else {
            mGameObjects.EmplaceBack();
            index = static_cast<u32>(mGameObjects.Size() - 1);
        }

        auto handle = mGameObjectPool.Allocate();
        if (!handle) {
            mFreeGameObjects.PushBack(index);
            return {};
        }

        auto& slot  = mGameObjects[index];
        slot.Handle = Move(handle);
        slot.Alive  = true;
        if (slot.Generation == 0) {
            slot.Generation = 1;
        }

        FGameObjectId id{};
        id.Index      = index;
        id.Generation = slot.Generation;
        id.WorldId    = mWorldId;

        auto* obj = slot.Handle.Get();
        obj->SetWorld(this);
        obj->SetId(id);
        obj->SetName(name);
        obj->SetActive(true);
        return id;
    }

    void FWorld::DestroyGameObject(FGameObjectId id) {
        auto* obj = ResolveGameObject(id);
        if (obj == nullptr) {
            return;
        }

        auto components = obj->GetAllComponents();
        for (const auto& componentId : components) {
            DestroyComponent(componentId);
        }

        auto& slot = mGameObjects[id.Index];
        mGameObjectPool.Deallocate(Move(slot.Handle));
        slot.Alive = false;
        slot.Generation++;
        if (slot.Generation == 0) {
            slot.Generation = 1;
        }
        mFreeGameObjects.PushBack(id.Index);
    }

    auto FWorld::IsAlive(FGameObjectId id) const noexcept -> bool {
        if (!id.IsValid() || id.WorldId != mWorldId) {
            return false;
        }
        if (id.Index >= static_cast<u32>(mGameObjects.Size())) {
            return false;
        }
        const auto& slot = mGameObjects[id.Index];
        return slot.Alive && slot.Generation == id.Generation && slot.Handle;
    }

    auto FWorld::CreateComponent(FGameObjectId owner, FComponentTypeHash type) -> FComponentId {
        if (!IsAlive(owner)) {
            return {};
        }

        FComponentCreateContext ctx{};
        ctx.World = this;
        ctx.Owner = owner;
        return GetComponentRegistry().Create(type, ctx);
    }

    void FWorld::DestroyComponent(FComponentId id) {
        if (!id.IsValid()) {
            return;
        }

        auto* storage = FindComponentStorage(id.Type);
        if (storage == nullptr) {
            return;
        }
        storage->Destroy(*this, id);
    }

    auto FWorld::IsAlive(FComponentId id) const noexcept -> bool {
        if (!id.IsValid()) {
            return false;
        }
        auto* storage = FindComponentStorage(id.Type);
        return storage != nullptr && storage->IsAlive(id);
    }

    auto FWorld::GetAllComponents(FGameObjectId owner) const -> TVector<FComponentId> {
        const auto* obj = ResolveGameObject(owner);
        if (obj == nullptr) {
            return {};
        }
        return obj->GetAllComponents();
    }

    void FWorld::SetGameObjectActive(FGameObjectId id, bool active) {
        auto* obj = ResolveGameObject(id);
        if (obj == nullptr) {
            return;
        }
        const bool wasActive = obj->IsActive();
        if (wasActive == active) {
            return;
        }
        obj->SetActive(active);
        OnGameObjectActiveChanged(id, active);
    }

    auto FWorld::IsGameObjectActive(FGameObjectId id) const -> bool {
        const auto* obj = ResolveGameObject(id);
        if (obj == nullptr) {
            return false;
        }
        return obj->IsActive();
    }

    void FWorld::UpdateTransforms() {
        ++mTransformUpdateId;
        if (mTransformUpdateId == 0) {
            mTransformUpdateId = 1;
        }

        for (u32 index = 0; index < static_cast<u32>(mGameObjects.Size()); ++index) {
            if (!mGameObjects[index].Alive) {
                continue;
            }
            FGameObjectId id{};
            id.Index      = index;
            id.Generation = mGameObjects[index].Generation;
            id.WorldId    = mWorldId;
            UpdateTransformRecursive(id, mTransformUpdateId);
        }
    }

    auto FWorld::GetActiveCameraComponents() const noexcept -> const TVector<FComponentId>& {
        return mActiveCameraComponents;
    }

    auto FWorld::GetActiveStaticMeshComponents() const noexcept -> const TVector<FComponentId>& {
        return mActiveStaticMeshComponents;
    }

    auto FWorld::GetActiveMeshMaterialComponents() const noexcept
        -> const TVector<FComponentId>& {
        return mActiveMeshMaterialComponents;
    }

    void FWorld::AddActiveComponent(TVector<FComponentId>& list, FComponentId id) {
        const auto count = static_cast<u32>(list.Size());
        for (u32 index = 0; index < count; ++index) {
            if (list[index] == id) {
                return;
            }
        }
        list.PushBack(id);
    }

    void FWorld::RemoveActiveComponent(TVector<FComponentId>& list, FComponentId id) {
        const auto count = static_cast<u32>(list.Size());
        for (u32 index = 0; index < count; ++index) {
            if (list[index] == id) {
                if (index != count - 1) {
                    list[index] = list[count - 1];
                }
                list.PopBack();
                return;
            }
        }
    }

    void FWorld::OnComponentCreated(FComponentId id, FGameObjectId owner) {
        if (!IsAlive(id) || !IsGameObjectActive(owner)) {
            return;
        }

        if (id.Type == kCameraComponentType) {
            const auto& component = ResolveComponent<FCameraComponent>(id);
            if (component.IsEnabled()) {
                AddActiveComponent(mActiveCameraComponents, id);
            }
            return;
        }

        if (id.Type == kStaticMeshComponentType) {
            const auto& component = ResolveComponent<FStaticMeshFilterComponent>(id);
            if (component.IsEnabled()) {
                AddActiveComponent(mActiveStaticMeshComponents, id);
            }
            return;
        }

        if (id.Type == kMeshMaterialComponentType) {
            const auto& component = ResolveComponent<FMeshMaterialComponent>(id);
            if (component.IsEnabled()) {
                AddActiveComponent(mActiveMeshMaterialComponents, id);
            }
        }
    }

    void FWorld::OnComponentDestroyed(FComponentId id, FGameObjectId /*owner*/) {
        if (id.Type == kCameraComponentType) {
            RemoveActiveComponent(mActiveCameraComponents, id);
            return;
        }

        if (id.Type == kStaticMeshComponentType) {
            RemoveActiveComponent(mActiveStaticMeshComponents, id);
            return;
        }

        if (id.Type == kMeshMaterialComponentType) {
            RemoveActiveComponent(mActiveMeshMaterialComponents, id);
        }
    }

    void FWorld::OnComponentEnabledChanged(
        FComponentId id, FGameObjectId owner, bool enabled) {
        if (id.Type == kCameraComponentType) {
            if (enabled && IsGameObjectActive(owner)) {
                AddActiveComponent(mActiveCameraComponents, id);
            } else {
                RemoveActiveComponent(mActiveCameraComponents, id);
            }
            return;
        }

        if (id.Type == kStaticMeshComponentType) {
            if (enabled && IsGameObjectActive(owner)) {
                AddActiveComponent(mActiveStaticMeshComponents, id);
            } else {
                RemoveActiveComponent(mActiveStaticMeshComponents, id);
            }
            return;
        }

        if (id.Type == kMeshMaterialComponentType) {
            if (enabled && IsGameObjectActive(owner)) {
                AddActiveComponent(mActiveMeshMaterialComponents, id);
            } else {
                RemoveActiveComponent(mActiveMeshMaterialComponents, id);
            }
        }
    }

    void FWorld::OnGameObjectActiveChanged(FGameObjectId owner, bool active) {
        auto* obj = ResolveGameObject(owner);
        if (obj == nullptr) {
            return;
        }

        const auto components = obj->GetAllComponents();
        for (const auto& id : components) {
            if (!IsAlive(id)) {
                continue;
            }

            if (id.Type == kCameraComponentType) {
                if (active && ResolveComponent<FCameraComponent>(id).IsEnabled()) {
                    AddActiveComponent(mActiveCameraComponents, id);
                } else {
                    RemoveActiveComponent(mActiveCameraComponents, id);
                }
                continue;
            }

            if (id.Type == kStaticMeshComponentType) {
                if (active && ResolveComponent<FStaticMeshFilterComponent>(id).IsEnabled()) {
                    AddActiveComponent(mActiveStaticMeshComponents, id);
                } else {
                    RemoveActiveComponent(mActiveStaticMeshComponents, id);
                }
                continue;
            }

            if (id.Type == kMeshMaterialComponentType) {
                if (active && ResolveComponent<FMeshMaterialComponent>(id).IsEnabled()) {
                    AddActiveComponent(mActiveMeshMaterialComponents, id);
                } else {
                    RemoveActiveComponent(mActiveMeshMaterialComponents, id);
                }
            }
        }
    }

    void FWorld::LinkComponentToOwner(FGameObjectId owner, FComponentId id) {
        auto* obj = ResolveGameObject(owner);
        if (obj == nullptr) {
            return;
        }
        obj->AddComponentId(id);
    }

    void FWorld::UnlinkComponentFromOwner(FGameObjectId owner, FComponentId id) {
        auto* obj = ResolveGameObject(owner);
        if (obj == nullptr) {
            return;
        }
        obj->RemoveComponentId(id);
    }

    auto FWorld::ResolveGameObject(FGameObjectId id) -> FGameObject* {
        if (!IsAlive(id)) {
            return nullptr;
        }
        return mGameObjects[id.Index].Handle.Get();
    }

    auto FWorld::ResolveGameObject(FGameObjectId id) const -> const FGameObject* {
        if (!IsAlive(id)) {
            return nullptr;
        }
        return mGameObjects[id.Index].Handle.Get();
    }

    auto FWorld::UpdateTransformRecursive(FGameObjectId id, u32 updateId) -> bool {
        auto* obj = ResolveGameObject(id);
        if (obj == nullptr) {
            return false;
        }

        if (obj->mTransformUpdateId == updateId) {
            return obj->mTransformChangedId == updateId;
        }

        bool              parentChanged = false;
        const auto        parentId      = obj->mParent;
        const FGameObject* parentObj     = nullptr;
        if (parentId.IsValid()) {
            parentChanged = UpdateTransformRecursive(parentId, updateId);
            parentObj     = ResolveGameObject(parentId);
        }

        const bool shouldUpdate = obj->mTransformDirty || parentChanged;
        if (shouldUpdate) {
            if (parentObj != nullptr) {
                obj->UpdateWorldTransform(parentObj->GetWorldTransform());
            } else {
                obj->UpdateWorldTransform();
            }
            obj->mTransformChangedId = updateId;
        }

        obj->mTransformUpdateId = updateId;
        return shouldUpdate;
    }

    auto FWorld::FindComponentStorage(FComponentTypeHash type) const -> FComponentStorageBase* {
        auto it = mComponentStorage.find(type);
        if (it == mComponentStorage.end()) {
            return nullptr;
        }
        return it->second.Get();
    }

    void FGameObject::SetName(FStringView name) { mName = FString(name); }

    auto FGameObject::AddComponentByType(FComponentTypeHash type) -> FComponentId {
        if (mWorld == nullptr) {
            return {};
        }
        return mWorld->CreateComponent(mId, type);
    }

    void FGameObject::RemoveComponent(FComponentId id) {
        if (mWorld == nullptr) {
            return;
        }
        mWorld->DestroyComponent(id);
    }

    auto FGameObject::GetAllComponents() const -> TVector<FComponentId> { return mComponents; }

    void FGameObject::AddComponentId(FComponentId id) { mComponents.PushBack(id); }

    void FGameObject::RemoveComponentId(FComponentId id) {
        const auto count = static_cast<u32>(mComponents.Size());
        for (u32 index = 0; index < count; ++index) {
            if (mComponents[index] == id) {
                if (index != count - 1) {
                    mComponents[index] = mComponents[count - 1];
                }
                mComponents.PopBack();
                return;
            }
        }
    }

    auto FGameObjectView::IsValid() const noexcept -> bool {
        return mWorld != nullptr && mWorld->IsAlive(mId);
    }

    void FGameObjectView::SetActive(bool active) {
        if (mWorld == nullptr) {
            return;
        }
        mWorld->SetGameObjectActive(mId, active);
    }

    auto FGameObjectView::IsActive() const -> bool {
        return mWorld != nullptr && mWorld->IsGameObjectActive(mId);
    }

    auto FGameObjectView::GetLocalTransform() const noexcept -> LinAlg::FSpatialTransform {
        if (mWorld == nullptr) {
            return LinAlg::FSpatialTransform::Identity();
        }
        const auto* obj = mWorld->ResolveGameObject(mId);
        if (obj == nullptr) {
            return LinAlg::FSpatialTransform::Identity();
        }
        return obj->GetLocalTransform();
    }

    auto FGameObjectView::GetWorldTransform() const noexcept -> LinAlg::FSpatialTransform {
        if (mWorld == nullptr) {
            return LinAlg::FSpatialTransform::Identity();
        }
        const auto* obj = mWorld->ResolveGameObject(mId);
        if (obj == nullptr) {
            return LinAlg::FSpatialTransform::Identity();
        }
        return obj->GetWorldTransform();
    }

    void FGameObjectView::SetLocalTransform(const LinAlg::FSpatialTransform& transform) {
        if (mWorld == nullptr) {
            return;
        }
        auto* obj = mWorld->ResolveGameObject(mId);
        if (obj == nullptr) {
            return;
        }
        obj->SetLocalTransform(transform);
    }

    void FGameObjectView::SetWorldTransform(const LinAlg::FSpatialTransform& transform) {
        if (mWorld == nullptr) {
            return;
        }
        auto* obj = mWorld->ResolveGameObject(mId);
        if (obj == nullptr) {
            return;
        }
        obj->SetWorldTransform(transform);
    }

    auto FGameObjectView::GetParent() const noexcept -> FGameObjectId {
        if (mWorld == nullptr) {
            return {};
        }
        const auto* obj = mWorld->ResolveGameObject(mId);
        if (obj == nullptr) {
            return {};
        }
        return obj->GetParent();
    }

    void FGameObjectView::SetParent(FGameObjectId parent) {
        if (mWorld == nullptr) {
            return;
        }
        auto* obj = mWorld->ResolveGameObject(mId);
        if (obj == nullptr) {
            return;
        }
        obj->SetParent(parent);
    }

    void FGameObjectView::ClearParent() {
        if (mWorld == nullptr) {
            return;
        }
        auto* obj = mWorld->ResolveGameObject(mId);
        if (obj == nullptr) {
            return;
        }
        obj->ClearParent();
    }
} // namespace AltinaEngine::GameScene
