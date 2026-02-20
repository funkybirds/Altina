#include "Engine/Runtime/MaterialCache.h"

#include "Material/Material.h"
#include "Material/MaterialTemplate.h"

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

    void FMaterialCache::PrepareMaterialForRendering(Render::FMaterial& material) {
        material.InitResource();
    }

    void FMaterialCache::Clear() {
        mFallbackMaterial.Reset();
    }
} // namespace AltinaEngine::Engine
