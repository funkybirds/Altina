#include "Engine/GameSceneAsset/Prefab.h"

#include "Logging/Log.h"

namespace AltinaEngine::Engine::GameSceneAsset {
    FBasePrefabInstantiator::FBasePrefabInstantiator(FNativeStringView loaderType)
        : mLoaderType(loaderType) {}

    auto FBasePrefabInstantiator::GetLoaderType() const -> FNativeStringView {
        return mLoaderType.ToView();
    }

    void FPrefabInstantiatorRegistry::Register(IPrefabInstantiator& instantiator) {
        const auto loaderType = instantiator.GetLoaderType();
        if (loaderType.IsEmpty()) {
            LogWarningCat(TEXT("Engine.GameSceneAsset"),
                TEXT("Prefab registry: ignored registration with empty loader type."));
            return;
        }

        const FNativeString key(loaderType);
        const bool          existed = mInstantiators.HasKey(key);
        mInstantiators[key]         = &instantiator;
        if (existed) {
            LogWarningCat(TEXT("Engine.GameSceneAsset"),
                TEXT("Prefab registry: replaced existing instantiator for loader type."));
        }
    }

    auto FPrefabInstantiatorRegistry::Find(FNativeStringView loaderType) const
        -> IPrefabInstantiator* {
        if (loaderType.IsEmpty()) {
            return nullptr;
        }

        const FNativeString key(loaderType);
        auto                it = mInstantiators.FindIt(key);
        if (it == mInstantiators.end()) {
            return nullptr;
        }
        return it->second;
    }

    auto FPrefabInstantiatorRegistry::Instantiate(FNativeStringView loaderType,
        GameScene::FWorld& world, Asset::FAssetManager& manager,
        const Asset::FAssetHandle& assetHandle) const -> FPrefabInstantiateResult {
        auto* instantiator = Find(loaderType);
        if (instantiator == nullptr) {
            LogWarningCat(TEXT("Engine.GameSceneAsset"),
                TEXT("Prefab registry: no instantiator found for requested loader type."));
            return {};
        }

        return instantiator->Instantiate(world, manager, assetHandle);
    }

    auto GetPrefabInstantiatorRegistry() -> FPrefabInstantiatorRegistry& {
        static FPrefabInstantiatorRegistry registry{};
        return registry;
    }
} // namespace AltinaEngine::Engine::GameSceneAsset
