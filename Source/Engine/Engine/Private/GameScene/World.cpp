#include "Engine/GameScene/World.h"

namespace AltinaEngine::GameScene {
    namespace {
        Core::Threading::TAtomic<u32> gNextWorldId(1);

        auto AcquireWorldId() -> u32 { return gNextWorldId.FetchAdd(1); }
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

    auto FWorld::CreateGameObject(Container::FStringView name) -> FGameObjectId {
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
        slot.Handle = AltinaEngine::Move(handle);
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
        mGameObjectPool.Deallocate(AltinaEngine::Move(slot.Handle));
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
        obj->SetActive(active);
    }

    auto FWorld::IsGameObjectActive(FGameObjectId id) const -> bool {
        const auto* obj = ResolveGameObject(id);
        if (obj == nullptr) {
            return false;
        }
        return obj->IsActive();
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

    auto FWorld::FindComponentStorage(FComponentTypeHash type) const -> FComponentStorageBase* {
        auto it = mComponentStorage.find(type);
        if (it == mComponentStorage.end()) {
            return nullptr;
        }
        return it->second.Get();
    }

    void FGameObject::SetName(FStringView name) {
        mName = FString(name);
    }

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

    auto FGameObject::GetAllComponents() const -> TVector<FComponentId> {
        return mComponents;
    }

    void FGameObject::AddComponentId(FComponentId id) {
        mComponents.PushBack(id);
    }

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
} // namespace AltinaEngine::GameScene





