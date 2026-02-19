#include "Engine/Runtime/MaterialCache.h"

#include "Asset/AssetBinary.h"
#include "Asset/Texture2DAsset.h"
#include "Material/Material.h"
#include "Material/MaterialTemplate.h"
#include "Math/Vector.h"
#include "Rhi/RhiDevice.h"
#include "Rhi/RhiInit.h"

using AltinaEngine::Move;
namespace AltinaEngine::Engine {
    namespace {
        auto ToRhiFormat(const Asset::FTexture2DDesc& desc) noexcept -> Rhi::ERhiFormat {
            const bool srgb = desc.SRGB;
            switch (desc.Format) {
                case Asset::kTextureFormatRGBA8:
                    return srgb ? Rhi::ERhiFormat::R8G8B8A8UnormSrgb
                                : Rhi::ERhiFormat::R8G8B8A8Unorm;
                case Asset::kTextureFormatRGB8:
                case Asset::kTextureFormatR8:
                    return srgb ? Rhi::ERhiFormat::R8G8B8A8UnormSrgb
                                : Rhi::ERhiFormat::R8G8B8A8Unorm;
                default:
                    return Rhi::ERhiFormat::Unknown;
            }
        }
    } // namespace

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

        Container::TVector<FTextureBinding> bindings;
        auto material = BuildMaterialFromAsset(*materialAsset, bindings);
        if (!material) {
            return ResolveDefault();
        }

        FEntry entry{};
        entry.Handle          = handle;
        entry.Material        = material;
        entry.TextureBindings = Move(bindings);
        mEntries.PushBack(Move(entry));
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

    void FMaterialCache::PrepareMaterialForRendering(Render::FMaterial& material) {
        if (auto* entry = FindEntryByMaterial(&material)) {
            ApplyTextureBindings(material, entry->TextureBindings);
        }

        material.InitResource();
    }

    void FMaterialCache::Clear() {
        mEntries.Clear();
        mTextureHandles.Clear();
        mTextureSRVs.Clear();
        mDefaultSampler.Reset();
    }

    auto FMaterialCache::FindEntryIndex(const Asset::FAssetHandle& handle) const noexcept -> isize {
        for (usize i = 0; i < mEntries.Size(); ++i) {
            if (mEntries[i].Handle == handle) {
                return static_cast<isize>(i);
            }
        }
        return -1;
    }

    auto FMaterialCache::FindEntryByMaterial(const Render::FMaterial* material) noexcept
        -> FEntry* {
        if (material == nullptr) {
            return nullptr;
        }
        for (auto& entry : mEntries) {
            if (entry.Material.Get() == material) {
                return &entry;
            }
        }
        return nullptr;
    }

    auto FMaterialCache::FindTextureEntryIndex(const Asset::FAssetHandle& handle) const noexcept
        -> isize {
        for (usize i = 0; i < mTextureHandles.Size(); ++i) {
            if (mTextureHandles[i] == handle) {
                return static_cast<isize>(i);
            }
        }
        return -1;
    }

    auto FMaterialCache::ResolveTextureEntry(const Asset::FAssetHandle& handle)
        -> Rhi::FRhiShaderResourceViewRef {
        if (!handle.IsValid() || handle.Type != Asset::EAssetType::Texture2D) {
            return {};
        }

        const isize existingIndex = FindTextureEntryIndex(handle);
        if (existingIndex >= 0) {
            auto& cachedSrv = mTextureSRVs[static_cast<usize>(existingIndex)];
            if (cachedSrv) {
                return cachedSrv;
            }
        }

        if (mAssetManager == nullptr) {
            return {};
        }

        auto asset = mAssetManager->Load(handle);
        if (!asset) {
            return {};
        }

        auto* textureAsset = static_cast<Asset::FTexture2DAsset*>(asset.Get());
        if (textureAsset == nullptr) {
            return {};
        }

        const auto& assetDesc = textureAsset->GetDesc();
        if (assetDesc.Width == 0U || assetDesc.Height == 0U) {
            return {};
        }

        Rhi::FRhiTextureDesc desc{};
        desc.mWidth       = assetDesc.Width;
        desc.mHeight      = assetDesc.Height;
        desc.mDepth       = 1U;
        desc.mMipLevels   = (assetDesc.MipCount > 0U) ? assetDesc.MipCount : 1U;
        desc.mArrayLayers = 1U;
        desc.mSampleCount = 1U;
        desc.mUsage       = Rhi::ERhiResourceUsage::Default;
        desc.mBindFlags   = Rhi::ERhiTextureBindFlags::ShaderResource;
        desc.mFormat      = ToRhiFormat(assetDesc);

        if (desc.mFormat == Rhi::ERhiFormat::Unknown) {
            return {};
        }

        auto texture = Rhi::RHICreateTexture(desc);
        if (!texture) {
            return {};
        }

        auto* device = Rhi::RHIGetDevice();
        if (device == nullptr) {
            return {};
        }

        Rhi::FRhiShaderResourceViewDesc srvDesc{};
        srvDesc.mTexture = texture.Get();
        srvDesc.mFormat  = desc.mFormat;

        auto srv = device->CreateShaderResourceView(srvDesc);
        if (existingIndex >= 0) {
            mTextureSRVs[static_cast<usize>(existingIndex)] = srv;
            return srv;
        }

        mTextureHandles.PushBack(handle);
        mTextureSRVs.PushBack(srv);
        return srv;
    }

    auto FMaterialCache::BuildMaterialFromAsset(const Asset::FMaterialAsset& asset,
        Container::TVector<FTextureBinding>& outBindings) const
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
            material->SetTexture(param.NameHash, {}, {}, param.SamplerFlags);
            outBindings.PushBack({ param.NameHash, param.Texture, param.SamplerFlags });
        }

        return material;
    }

    void FMaterialCache::ApplyTextureBindings(Render::FMaterial& material,
        const Container::TVector<FTextureBinding>& bindings) {
        if (bindings.IsEmpty()) {
            return;
        }

        if (!mDefaultSampler) {
            Rhi::FRhiSamplerDesc samplerDesc{};
            mDefaultSampler = Rhi::RHICreateSampler(samplerDesc);
        }

        for (const auto& binding : bindings) {
            if (binding.NameHash == 0U) {
                continue;
            }

            auto srv = ResolveTextureEntry(binding.Texture);
            material.SetTexture(binding.NameHash, srv, mDefaultSampler, binding.SamplerFlags);
        }
    }
} // namespace AltinaEngine::Engine
