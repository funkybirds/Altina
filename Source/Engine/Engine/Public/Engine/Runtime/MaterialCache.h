#pragma once

#include "Engine/EngineAPI.h"

#include "Asset/AssetManager.h"
#include "Asset/MaterialAsset.h"
#include "Container/SmartPtr.h"
#include "Container/Vector.h"
#include "Material/Material.h"
#include "Material/MaterialTemplate.h"
#include "Rhi/RhiRefs.h"
#include "Types/Aliases.h"
#include "Types/Traits.h"

namespace AltinaEngine::Engine {
    using AltinaEngine::Move;
    namespace Container = Core::Container;
    namespace Asset     = AltinaEngine::Asset;
    namespace Render    = AltinaEngine::RenderCore;

    /**
     * @brief Cache mapping material assets to render-core materials.
     */
    class AE_ENGINE_API FMaterialCache {
    public:
        void SetAssetManager(Asset::FAssetManager* manager) noexcept { mAssetManager = manager; }
        void SetDefaultMaterial(Render::FMaterial* material) noexcept { mDefaultMaterial = material; }
        void SetDefaultTemplate(Container::TShared<Render::FMaterialTemplate> templ) noexcept {
            mDefaultTemplate = Move(templ);
        }

        [[nodiscard]] auto Resolve(const Asset::FAssetHandle* handle) -> Render::FMaterial*;
        [[nodiscard]] auto Resolve(const Asset::FAssetHandle& handle) -> Render::FMaterial*;
        [[nodiscard]] auto ResolveDefault() -> Render::FMaterial*;
        void               PrepareMaterialForRendering(Render::FMaterial& material);

        void Clear();

    private:
        struct FTextureBinding {
            Render::FMaterialParamId NameHash    = 0U;
            Asset::FAssetHandle      Texture{};
            u32                      SamplerFlags = 0U;
        };

        struct FEntry {
            Asset::FAssetHandle                 Handle{};
            Container::TShared<Render::FMaterial> Material;
            Container::TVector<FTextureBinding> TextureBindings;
        };

        [[nodiscard]] auto FindEntryIndex(const Asset::FAssetHandle& handle) const noexcept -> isize;
        [[nodiscard]] auto FindEntryByMaterial(const Render::FMaterial* material) noexcept
            -> FEntry*;
        [[nodiscard]] auto FindTextureEntryIndex(const Asset::FAssetHandle& handle) const noexcept
            -> isize;
        auto               ResolveTextureEntry(const Asset::FAssetHandle& handle)
            -> Rhi::FRhiShaderResourceViewRef;
        auto               BuildMaterialFromAsset(const Asset::FMaterialAsset& asset,
                          Container::TVector<FTextureBinding>& outBindings) const
            -> Container::TShared<Render::FMaterial>;
        void               ApplyTextureBindings(Render::FMaterial& material,
                          const Container::TVector<FTextureBinding>& bindings);

        Asset::FAssetManager*                     mAssetManager = nullptr;
        Container::TShared<Render::FMaterialTemplate> mDefaultTemplate;
        Render::FMaterial*                        mDefaultMaterial = nullptr;
        Container::TShared<Render::FMaterial>     mFallbackMaterial;
        Container::TVector<FEntry>                mEntries;
        Container::TVector<Asset::FAssetHandle>   mTextureHandles;
        Container::TVector<Rhi::FRhiShaderResourceViewRef> mTextureSRVs;
        Rhi::FRhiSamplerRef                       mDefaultSampler;
    };
} // namespace AltinaEngine::Engine
