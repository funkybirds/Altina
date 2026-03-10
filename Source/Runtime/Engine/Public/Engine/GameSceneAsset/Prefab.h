#pragma once

#include "Engine/EngineAPI.h"
#include "Engine/GameScene/Ids.h"
#include "Asset/AssetTypes.h"
#include "Container/HashMap.h"
#include "Container/String.h"
#include "Container/StringView.h"
#include "Container/Vector.h"

namespace AltinaEngine::Asset {
    class FAssetManager;
} // namespace AltinaEngine::Asset

namespace AltinaEngine::GameScene {
    class FWorld;
} // namespace AltinaEngine::GameScene

namespace AltinaEngine::Engine::GameSceneAsset {
    namespace Container = Core::Container;
    using Container::FNativeString;
    using Container::FNativeStringView;
    using Container::THashMap;
    using Container::TVector;

    struct AE_ENGINE_API FPrefabDescriptor {
        FNativeString       LoaderType{};
        Asset::FAssetHandle AssetHandle{};
    };

    struct AE_ENGINE_API FPrefabInstantiateResult {
        GameScene::FGameObjectId          Root{};
        TVector<GameScene::FGameObjectId> SpawnedNodes{};
    };

    class AE_ENGINE_API IPrefabInstantiator {
    public:
        virtual ~IPrefabInstantiator() = default;

        [[nodiscard]] virtual auto GetLoaderType() const -> FNativeStringView   = 0;
        virtual auto Instantiate(GameScene::FWorld& world, Asset::FAssetManager& manager,
            const Asset::FAssetHandle& assetHandle) -> FPrefabInstantiateResult = 0;
    };

    class AE_ENGINE_API FBasePrefabInstantiator : public IPrefabInstantiator {
    public:
        explicit FBasePrefabInstantiator(FNativeStringView loaderType);
        ~FBasePrefabInstantiator() override = default;

        [[nodiscard]] auto GetLoaderType() const -> FNativeStringView override;

    private:
        FNativeString mLoaderType{};
    };

    class AE_ENGINE_API FPrefabInstantiatorRegistry final {
    public:
        void               Register(IPrefabInstantiator& instantiator);

        [[nodiscard]] auto Find(FNativeStringView loaderType) const -> IPrefabInstantiator*;
        [[nodiscard]] auto Instantiate(FNativeStringView loaderType, GameScene::FWorld& world,
            Asset::FAssetManager& manager, const Asset::FAssetHandle& assetHandle) const
            -> FPrefabInstantiateResult;

    private:
        THashMap<FNativeString, IPrefabInstantiator*> mInstantiators{};
    };

    AE_ENGINE_API auto GetPrefabInstantiatorRegistry() -> FPrefabInstantiatorRegistry&;
} // namespace AltinaEngine::Engine::GameSceneAsset
