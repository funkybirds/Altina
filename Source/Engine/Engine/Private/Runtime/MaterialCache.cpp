#include "Engine/Runtime/MaterialCache.h"

#include "Material/Material.h"
#include "Material/MaterialTemplate.h"
#include "Math/Vector.h"

using AltinaEngine::Move;
namespace AltinaEngine::Engine {
    auto FMaterialCache::Resolve(const Asset::FAssetHandle* handle) -> Render::FMaterial* {
        if (handle == nullptr) {
            return ResolveDefault();
        }
        return Resolve(*handle);
    }

    auto FMaterialCache::Resolve(const Asset::FAssetHandle& handle) -> Render::FMaterial* {
        if (!handle.IsValid()) {
            return ResolveDefault();
        }

        const isize index = FindEntryIndex(handle);
        if (index >= 0) {
            return mEntries[static_cast<usize>(index)].Material.Get();
        }

        if (mAssetManager == nullptr) {
            return ResolveDefault();
        }

        auto asset = mAssetManager->Load(handle);
        if (!asset) {
            return ResolveDefault();
        }

        if (handle.Type != Asset::EAssetType::Material) {
            return ResolveDefault();
        }

        auto* materialAsset = static_cast<Asset::FMaterialAsset*>(asset.Get());
        if (materialAsset == nullptr) {
            return ResolveDefault();
        }

        auto material = CreateMaterialFromAsset(*materialAsset);
        if (!material) {
            return ResolveDefault();
        }

        mEntries.PushBack({ handle, material });
        return material.Get();
    }

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

    void FMaterialCache::Clear() { mEntries.Clear(); }

    auto FMaterialCache::FindEntryIndex(const Asset::FAssetHandle& handle) const noexcept -> isize {
        for (usize i = 0; i < mEntries.Size(); ++i) {
            if (mEntries[i].Handle == handle) {
                return static_cast<isize>(i);
            }
        }
        return -1;
    }

    auto FMaterialCache::CreateMaterialFromAsset(const Asset::FMaterialAsset& asset) const
        -> Container::TShared<Render::FMaterial> {
        auto material = Container::MakeShared<Render::FMaterial>();
        Render::FMaterialDesc desc{};
        const auto&           assetDesc = asset.GetDesc();
        desc.ShadingModel               = assetDesc.ShadingModel;
        desc.BlendMode                  = assetDesc.BlendMode;
        desc.Flags                      = assetDesc.Flags;
        desc.AlphaCutoff                = assetDesc.AlphaCutoff;
        material->SetDesc(desc);

        if (mDefaultTemplate) {
            material->SetTemplate(mDefaultTemplate);
        }

        for (const auto& param : asset.GetScalars()) {
            material->SetScalar(param.NameHash, param.Value);
        }

        for (const auto& param : asset.GetVectors()) {
            const auto& v = param.Value;
            Render::Math::FVector4f value(v[0], v[1], v[2], v[3]);
            material->SetVector(param.NameHash, value);
        }

        for (const auto& param : asset.GetTextures()) {
            // Texture SRV/Sampler resolution happens in the render path.
            material->SetTexture(param.NameHash, {}, {}, param.SamplerFlags);
        }

        return material;
    }
} // namespace AltinaEngine::Engine
