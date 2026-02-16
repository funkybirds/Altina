#pragma once

#include "Engine/EngineAPI.h"
#include "Engine/GameScene/Component.h"
#include "Engine/GameScene/ComponentRegistry.h"
#include "Engine/GameScene/GameObject.h"
#include "Engine/GameScene/GameObjectView.h"
#include "Container/HashMap.h"
#include "Container/SmartPtr.h"
#include "Container/Vector.h"
#include "Memory/ObjectPool.h"
#include "Threading/Atomic.h"
#include "Types/Traits.h"

using AltinaEngine::CClassBaseOf;
using AltinaEngine::Move;
using AltinaEngine::Core::Container::FStringView;
using AltinaEngine::Core::Container::MakeUniqueAs;
namespace AltinaEngine::GameScene {
    namespace Container = Core::Container;
    using Container::THashMap;
    using Container::TOwner;
    using Container::TPolymorphicDeleter;
    using Container::TVector;

    /**
     * @brief Runtime container for game objects and components.
     */
    class AE_ENGINE_API FWorld {
    public:
        FWorld();
        explicit FWorld(u32 worldId);
        ~FWorld();

        FWorld(const FWorld&)                             = delete;
        auto operator=(const FWorld&) -> FWorld&          = delete;
        FWorld(FWorld&&)                                  = delete;
        auto               operator=(FWorld&&) -> FWorld& = delete;

        [[nodiscard]] auto GetWorldId() const noexcept -> u32 { return mWorldId; }

        [[nodiscard]] auto CreateGameObject(FStringView name = {}) -> FGameObjectId;
        void               DestroyGameObject(FGameObjectId id);
        [[nodiscard]] auto IsAlive(FGameObjectId id) const noexcept -> bool;

        template <typename T>
        [[nodiscard]] auto CreateComponent(FGameObjectId owner) -> FComponentId;
        [[nodiscard]] auto CreateComponent(FGameObjectId owner, FComponentTypeHash type)
            -> FComponentId;
        void                                     DestroyComponent(FComponentId id);
        [[nodiscard]] auto                       IsAlive(FComponentId id) const noexcept -> bool;

        template <typename T> [[nodiscard]] auto HasComponent(FGameObjectId owner) const -> bool;
        template <typename T>
        [[nodiscard]] auto GetComponent(FGameObjectId owner) const -> FComponentId;
        [[nodiscard]] auto GetAllComponents(FGameObjectId owner) const -> TVector<FComponentId>;

        /**
         * @brief Resolve a component reference by ID.
         * @note Caller must ensure the ID is valid and matches the requested type.
         */
        template <typename T> [[nodiscard]] auto ResolveComponent(FComponentId id) -> T&;
        template <typename T>
        [[nodiscard]] auto ResolveComponent(FComponentId id) const -> const T&;

        [[nodiscard]] auto Object(FGameObjectId id) -> FGameObjectView {
            return FGameObjectView(this, id);
        }

        void               SetGameObjectActive(FGameObjectId id, bool active);
        [[nodiscard]] auto IsGameObjectActive(FGameObjectId id) const -> bool;

    private:
        struct FGameObjectSlot {
            Core::Memory::TObjectPoolHandle<FGameObject> Handle{};
            u32                                          Generation = 1;
            bool                                         Alive      = false;
        };

        class FComponentStorageBase {
        public:
            virtual ~FComponentStorageBase() = default;

            virtual void               Destroy(FWorld& world, FComponentId id) = 0;
            [[nodiscard]] virtual auto IsAlive(FComponentId id) const -> bool  = 0;
            virtual void               DestroyAll(FWorld& world)               = 0;
        };

        template <typename T> class TComponentStorage final : public FComponentStorageBase {
        public:
            auto Create(FWorld& world, FGameObjectId owner) -> FComponentId {
                auto handle = mPool.Allocate();
                if (!handle) {
                    return {};
                }

                const u32 index = AcquireSlot();
                auto&     slot  = mSlots[index];
                slot.Handle     = Move(handle);
                slot.Alive      = true;
                if (slot.Generation == 0) {
                    slot.Generation = 1;
                }
                slot.Owner = owner;

                FComponentId id{};
                id.Index      = index;
                id.Generation = slot.Generation;
                id.Type       = mTypeHash;

                auto* component = slot.Handle.Get();
                world.InitializeComponent(*component, id, owner);
                component->OnCreate();
                if (component->IsEnabled()) {
                    component->OnEnable();
                }

                world.LinkComponentToOwner(owner, id);
                return id;
            }

            void Destroy(FWorld& world, FComponentId id) override {
                if (!IsAlive(id)) {
                    return;
                }

                auto& slot  = mSlots[id.Index];
                auto  owner = slot.Owner;

                if (slot.Handle) {
                    auto* component = slot.Handle.Get();
                    if (component->IsEnabled()) {
                        component->OnDisable();
                    }
                    component->OnDestroy();
                }

                mPool.Deallocate(Move(slot.Handle));
                slot.Owner = {};
                slot.Alive = false;
                slot.Generation++;
                if (slot.Generation == 0) {
                    slot.Generation = 1;
                }
                mFreeList.PushBack(id.Index);

                world.UnlinkComponentFromOwner(owner, id);
            }

            [[nodiscard]] auto IsAlive(FComponentId id) const -> bool override {
                if (!id.IsValid() || id.Type != mTypeHash) {
                    return false;
                }
                if (id.Index >= static_cast<u32>(mSlots.Size())) {
                    return false;
                }
                const auto& slot = mSlots[id.Index];
                return slot.Alive && slot.Generation == id.Generation && slot.Handle;
            }

            void DestroyAll(FWorld& world) override {
                for (u32 index = 0; index < static_cast<u32>(mSlots.Size()); ++index) {
                    if (!mSlots[index].Alive) {
                        continue;
                    }
                    FComponentId id{};
                    id.Index      = index;
                    id.Generation = mSlots[index].Generation;
                    id.Type       = mTypeHash;
                    Destroy(world, id);
                }
            }

            [[nodiscard]] auto Resolve(FComponentId id) -> T& {
                return *mSlots[id.Index].Handle.Get();
            }
            [[nodiscard]] auto Resolve(FComponentId id) const -> const T& {
                return *mSlots[id.Index].Handle.Get();
            }

        private:
            struct FSlot {
                Core::Memory::TObjectPoolHandle<T> Handle{};
                u32                                Generation = 1;
                bool                               Alive      = false;
                FGameObjectId                      Owner{};
            };

            auto AcquireSlot() -> u32 {
                if (!mFreeList.IsEmpty()) {
                    const u32 index = mFreeList.Back();
                    mFreeList.PopBack();
                    return index;
                }
                mSlots.EmplaceBack();
                return static_cast<u32>(mSlots.Size() - 1);
            }

            Core::Memory::TThreadSafeObjectPool<T> mPool{};
            TVector<FSlot>                         mSlots{};
            TVector<u32>                           mFreeList{};
            const FComponentTypeHash               mTypeHash = GetComponentTypeHash<T>();
        };

        using FComponentStoragePtr =
            TOwner<FComponentStorageBase, TPolymorphicDeleter<FComponentStorageBase>>;

        template <typename T> friend class TComponentStorage;

        void InitializeComponent(FComponent& component, FComponentId id, FGameObjectId owner) {
            component.Initialize(id, owner);
        }

        void               LinkComponentToOwner(FGameObjectId owner, FComponentId id);
        void               UnlinkComponentFromOwner(FGameObjectId owner, FComponentId id);

        [[nodiscard]] auto ResolveGameObject(FGameObjectId id) -> FGameObject*;
        [[nodiscard]] auto ResolveGameObject(FGameObjectId id) const -> const FGameObject*;

        [[nodiscard]] auto FindComponentStorage(FComponentTypeHash type) const
            -> FComponentStorageBase*;

        template <typename T> void EnsureComponentRegistration();
        template <typename T>
        [[nodiscard]] auto FindComponentStorage() const -> TComponentStorage<T>*;
        template <typename T>
        [[nodiscard]] auto GetOrCreateComponentStorage() -> TComponentStorage<T>&;

        u32                mWorldId = 0;
        Core::Memory::TThreadSafeObjectPool<FGameObject>   mGameObjectPool{};
        TVector<FGameObjectSlot>                           mGameObjects{};
        TVector<u32>                                       mFreeGameObjects{};
        THashMap<FComponentTypeHash, FComponentStoragePtr> mComponentStorage{};
    };

    namespace Detail {
        template <typename T>
        auto CreateComponentThunk(FComponentCreateContext& ctx) -> FComponentId {
            if (ctx.World == nullptr) {
                return {};
            }
            return ctx.World->CreateComponent<T>(ctx.Owner);
        }

        template <typename T> void DestroyComponentThunk(FWorld& world, FComponentId id) {
            world.DestroyComponent(id);
        }
    } // namespace Detail

    // FWorld template methods
    template <typename T> inline auto FWorld::CreateComponent(FGameObjectId owner) -> FComponentId {
        static_assert(CClassBaseOf<FComponent, T>, "Component types must derive from FComponent.");

        if (!IsAlive(owner)) {
            return {};
        }

        EnsureComponentRegistration<T>();
        auto& storage = GetOrCreateComponentStorage<T>();
        return storage.Create(*this, owner);
    }

    template <typename T> inline auto FWorld::HasComponent(FGameObjectId owner) const -> bool {
        return GetComponent<T>(owner).IsValid();
    }

    template <typename T>
    inline auto FWorld::GetComponent(FGameObjectId owner) const -> FComponentId {
        const auto* obj = ResolveGameObject(owner);
        if (obj == nullptr) {
            return {};
        }

        const FComponentTypeHash typeHash = GetComponentTypeHash<T>();
        for (const auto& id : obj->mComponents) {
            if (id.Type == typeHash && IsAlive(id)) {
                return id;
            }
        }
        return {};
    }

    template <typename T> inline auto FWorld::ResolveComponent(FComponentId id) -> T& {
        auto* storage = FindComponentStorage<T>();
        return storage->Resolve(id);
    }

    template <typename T> inline auto FWorld::ResolveComponent(FComponentId id) const -> const T& {
        auto* storage = FindComponentStorage<T>();
        return storage->Resolve(id);
    }

    template <typename T> inline void FWorld::EnsureComponentRegistration() {
        const FComponentTypeHash typeHash = GetComponentTypeHash<T>();
        auto&                    registry = GetComponentRegistry();
        if (!registry.Has(typeHash)) {
            registry.Register(BuildComponentTypeEntry<T>());
        }
    }

    template <typename T>
    inline auto FWorld::FindComponentStorage() const -> TComponentStorage<T>* {
        const FComponentTypeHash typeHash = GetComponentTypeHash<T>();
        auto                     it       = mComponentStorage.find(typeHash);
        if (it == mComponentStorage.end()) {
            return nullptr;
        }
        return static_cast<TComponentStorage<T>*>(it->second.Get());
    }

    template <typename T>
    inline auto FWorld::GetOrCreateComponentStorage() -> TComponentStorage<T>& {
        if (auto* storage = FindComponentStorage<T>()) {
            return *storage;
        }

        const FComponentTypeHash typeHash = GetComponentTypeHash<T>();
        auto  storagePtr = MakeUniqueAs<FComponentStorageBase, TComponentStorage<T>>();
        auto* rawPtr     = static_cast<TComponentStorage<T>*>(storagePtr.Get());
        mComponentStorage.emplace(typeHash, Move(storagePtr));
        return *rawPtr;
    }

    // FGameObject inline methods
    template <typename T> inline auto FGameObject::AddComponent() -> FComponentId {
        if (mWorld == nullptr) {
            return {};
        }
        return mWorld->CreateComponent<T>(mId);
    }

    template <typename T> inline auto FGameObject::HasComponent() const -> bool {
        if (mWorld == nullptr) {
            return false;
        }
        return mWorld->HasComponent<T>(mId);
    }

    template <typename T> inline auto FGameObject::GetComponent() const -> FComponentId {
        if (mWorld == nullptr) {
            return {};
        }
        return mWorld->GetComponent<T>(mId);
    }

    // FGameObjectView inline methods
    template <typename T> inline auto FGameObjectView::Add() -> TComponentRef<T> {
        if (mWorld == nullptr) {
            return {};
        }
        const auto id = mWorld->CreateComponent<T>(mId);
        return TComponentRef<T>(mWorld, id);
    }

    template <typename T> inline auto FGameObjectView::Has() const -> bool {
        return mWorld != nullptr && mWorld->HasComponent<T>(mId);
    }

    template <typename T> inline auto FGameObjectView::Get() const -> TComponentRef<T> {
        if (mWorld == nullptr) {
            return {};
        }
        const auto id = mWorld->GetComponent<T>(mId);
        return TComponentRef<T>(mWorld, id);
    }

    template <typename T> inline void FGameObjectView::Remove() {
        if (mWorld == nullptr) {
            return;
        }
        const auto id = mWorld->GetComponent<T>(mId);
        if (id.IsValid()) {
            mWorld->DestroyComponent(id);
        }
    }

    // TComponentRef inline methods
    template <typename T> inline auto TComponentRef<T>::IsValid() const noexcept -> bool {
        return mWorld != nullptr && mWorld->IsAlive(mId);
    }

    template <typename T> inline auto TComponentRef<T>::Get() -> T& {
        return mWorld->ResolveComponent<T>(mId);
    }

    template <typename T> inline auto TComponentRef<T>::Get() const -> const T& {
        return mWorld->ResolveComponent<T>(mId);
    }
} // namespace AltinaEngine::GameScene
