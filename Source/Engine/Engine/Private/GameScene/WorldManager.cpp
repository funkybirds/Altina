#include "Engine/GameScene/WorldManager.h"

using AltinaEngine::Move;
using AltinaEngine::Core::Container::MakeUnique;
namespace AltinaEngine::GameScene {
    auto FWorldManager::CreateWorld() -> FWorldHandle {
        auto      world = MakeUnique<FWorld>();
        const u32 id    = world->GetWorldId();
        mWorlds.emplace(id, Move(world));

        FWorldHandle handle{ id };
        if (!mActiveWorld.IsValid()) {
            mActiveWorld = handle;
        }
        return handle;
    }

    void FWorldManager::DestroyWorld(FWorldHandle handle) {
        if (!handle.IsValid()) {
            return;
        }

        auto it = mWorlds.find(handle.Id);
        if (it == mWorlds.end()) {
            return;
        }

        if (mActiveWorld == handle) {
            mActiveWorld = {};
        }

        mWorlds.erase(it);
    }

    auto FWorldManager::GetWorld(FWorldHandle handle) noexcept -> FWorld* {
        if (!handle.IsValid()) {
            return nullptr;
        }

        auto it = mWorlds.find(handle.Id);
        if (it == mWorlds.end()) {
            return nullptr;
        }
        return it->second.Get();
    }

    auto FWorldManager::GetWorld(FWorldHandle handle) const noexcept -> const FWorld* {
        if (!handle.IsValid()) {
            return nullptr;
        }

        auto it = mWorlds.find(handle.Id);
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

        if (mWorlds.find(handle.Id) == mWorlds.end()) {
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
        mWorlds.clear();
        mActiveWorld = {};
    }
} // namespace AltinaEngine::GameScene
