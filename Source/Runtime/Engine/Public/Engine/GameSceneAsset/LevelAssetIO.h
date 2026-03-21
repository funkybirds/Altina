#pragma once

#include "Engine/EngineAPI.h"
#include "Container/SmartPtr.h"
#include "Container/String.h"
#include "Asset/AssetTypes.h"
#include "Engine/GameScene/World.h"

namespace AltinaEngine::Asset {
    class FAssetManager;
}

namespace AltinaEngine::Engine::GameSceneAsset {
    AE_ENGINE_API auto SaveWorldAsLevelJson(
        const GameScene::FWorld& world, Core::Container::FNativeString& outJson) -> bool;

    AE_ENGINE_API auto LoadWorldFromLevelAsset(const Asset::FAssetHandle& levelHandle,
        Asset::FAssetManager&                                             manager,
        GameScene::EWorldDeserializeMode mode = GameScene::EWorldDeserializeMode::NormalRuntime)
        -> Core::Container::TOwner<GameScene::FWorld>;
} // namespace AltinaEngine::Engine::GameSceneAsset
