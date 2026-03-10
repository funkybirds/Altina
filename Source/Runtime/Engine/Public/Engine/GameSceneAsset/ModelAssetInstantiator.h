#pragma once

#include "Engine/EngineAPI.h"
#include "Engine/GameSceneAsset/Prefab.h"

namespace AltinaEngine::Asset {
    class FAssetManager;
    struct FAssetHandle;
} // namespace AltinaEngine::Asset

namespace AltinaEngine::GameScene {
    class FWorld;
} // namespace AltinaEngine::GameScene

namespace AltinaEngine::Engine::GameSceneAsset {
    class AE_ENGINE_API FModelAssetInstantiator final : public FBasePrefabInstantiator {
    public:
        static constexpr const char* kLoaderType = "engine.prefab.model_asset";

        FModelAssetInstantiator();

        auto Instantiate(GameScene::FWorld& world, Asset::FAssetManager& manager,
            const Asset::FAssetHandle& modelHandle) -> FPrefabInstantiateResult override;
    };
} // namespace AltinaEngine::Engine::GameSceneAsset
