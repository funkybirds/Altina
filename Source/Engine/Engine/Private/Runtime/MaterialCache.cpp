#include "Engine/Runtime/MaterialCache.h"

#include "Material/Material.h"
#include "Material/MaterialTemplate.h"
#include "Engine/GameScene/MeshMaterialComponent.h"

using AltinaEngine::Move;
namespace AltinaEngine::Engine {
    auto FMaterialCache::ResolveDefault() -> Render::FMaterial* {
        if (mDefaultMaterial != nullptr) {
            return mDefaultMaterial;
        }

        if (!mFallbackMaterial) {
            auto fallback = Container::MakeShared<Render::FMaterial>();
            if (mDefaultTemplate) {
                fallback->SetTemplate(mDefaultTemplate);
            }
            mFallbackMaterial = Move(fallback);
        }

        return mFallbackMaterial.Get();
    }

    auto FMaterialCache::ResolveMaterial(const Asset::FAssetHandle& handle,
        const Asset::FMeshMaterialParameterBlock& parameters) -> Render::FMaterial* {
        if (!handle.IsValid()) {
            return nullptr;
        }

        if (!GameScene::FMeshMaterialComponent::AssetToRenderMaterialConverter) {
            return nullptr;
        }

        FMaterialCacheKey key{};
        key.Handle    = handle;
        key.ParamHash = parameters.GetHash();

        const auto it = mMaterialCache.find(key);
        if (it != mMaterialCache.end()) {
            return it->second.Get();
        }

        Render::FMaterial material =
            GameScene::FMeshMaterialComponent::AssetToRenderMaterialConverter(handle, parameters);
        auto  sharedMaterial = Container::MakeShared<Render::FMaterial>(Move(material));
        auto* rawPtr         = sharedMaterial.Get();
        mMaterialCache.emplace(Move(key), Move(sharedMaterial));
        return rawPtr;
    }

    void FMaterialCache::PrepareMaterialForRendering(Render::FMaterial& material) {
        material.InitResource();
    }

    void FMaterialCache::Clear() {
        mFallbackMaterial.Reset();
        mMaterialCache.clear();
    }
} // namespace AltinaEngine::Engine
