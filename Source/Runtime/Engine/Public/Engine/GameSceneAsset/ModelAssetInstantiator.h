#pragma once

#include "Engine/EngineAPI.h"
#include "Engine/GameScene/Ids.h"
#include "Container/Vector.h"

namespace AltinaEngine::Asset {
    class FAssetManager;
    struct FAssetHandle;
} // namespace AltinaEngine::Asset

namespace AltinaEngine::GameScene {
    class FWorld;
} // namespace AltinaEngine::GameScene

namespace AltinaEngine::Engine::GameSceneAsset {
    namespace Container = Core::Container;
    using Container::TVector;

    struct AE_ENGINE_API FModelInstantiateResult {
        GameScene::FGameObjectId          Root{};
        TVector<GameScene::FGameObjectId> Nodes{};
    };

    class AE_ENGINE_API FModelAssetInstantiator final {
    public:
        static auto Instantiate(GameScene::FWorld& world, Asset::FAssetManager& manager,
            const Asset::FAssetHandle& modelHandle) -> FModelInstantiateResult;
    };
} // namespace AltinaEngine::Engine::GameSceneAsset
