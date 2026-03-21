#include "Engine/GameScene/WorldManager.h"

using AltinaEngine::Move;
using AltinaEngine::Core::Container::MakeUnique;
namespace AltinaEngine::GameScene {
    auto FWorldManager::CreateWorld() -> FWorldHandle {
        auto      world = MakeUnique<FWorld>();
        const u32 id    = world->GetWorldId();
        mWorlds.Emplace(id, Move(world));

        FWorldHandle handle{ id };
        if (!mActiveWorld.IsValid()) {
            mActiveWorld = handle;
        }
        return handle;
    }

    auto FWorldManager::AddWorld(TOwner<FWorld> world) -> FWorldHandle {
        if (!world) {
            return {};
        }

        const u32 id = world->GetWorldId();
        if (id == 0U) {
            return {};
        }

        mWorlds[id] = Move(world);

        FWorldHandle handle{ id };
        if (!mActiveWorld.IsValid()) {
            mActiveWorld = handle;
        }
        return handle;
    }

    auto FWorldManager::ReplaceWorld(FWorldHandle handle, TOwner<FWorld> world) -> FWorldHandle {
        if (!handle.IsValid() || !world) {
            return {};
        }

        const u32 worldId = world->GetWorldId();
        if (worldId != handle.Id) {
            return {};
        }

        mWorlds[handle.Id] = Move(world);
        if (!mActiveWorld.IsValid() || mActiveWorld == handle) {
            mActiveWorld = handle;
        }
        return handle;
    }

    void FWorldManager::DestroyWorld(FWorldHandle handle) {
        if (!handle.IsValid()) {
            return;
        }

        auto it = mWorlds.FindIt(handle.Id);
        if (it == mWorlds.end()) {
            return;
        }

        if (mActiveWorld == handle) {
            mActiveWorld = {};
        }

        mWorlds.Erase(it);
    }

    auto FWorldManager::GetWorld(FWorldHandle handle) noexcept -> FWorld* {
        if (!handle.IsValid()) {
            return nullptr;
        }

        auto it = mWorlds.FindIt(handle.Id);
        if (it == mWorlds.end()) {
            return nullptr;
        }
        return it->second.Get();
    }

    auto FWorldManager::GetWorld(FWorldHandle handle) const noexcept -> const FWorld* {
        if (!handle.IsValid()) {
            return nullptr;
        }

        auto it = mWorlds.FindIt(handle.Id);
        if (it == mWorlds.end()) {
            return nullptr;
        }
        return it->second.Get();
    }

    void FWorldManager::SetActiveWorld(FWorldHandle handle) {
        if (!handle.IsValid()) {
            mActiveWorld = {};
            return;
        }

        if (mWorlds.FindIt(handle.Id) == mWorlds.end()) {
            return;
        }

        mActiveWorld = handle;
    }

    auto FWorldManager::GetActiveWorldHandle() const noexcept -> FWorldHandle {
        return mActiveWorld;
    }

    auto FWorldManager::GetActiveWorld() noexcept -> FWorld* { return GetWorld(mActiveWorld); }

    auto FWorldManager::GetActiveWorld() const noexcept -> const FWorld* {
        return GetWorld(mActiveWorld);
    }

    void FWorldManager::Clear() {
        mWorlds.Clear();
        mActiveWorld = {};
    }
} // namespace AltinaEngine::GameScene
