#pragma once

#include "Engine/EngineAPI.h"

#include "Asset/AssetManager.h"
#include "Asset/MaterialAsset.h"
#include "Container/SmartPtr.h"
#include "Container/Vector.h"
#include "Material/Material.h"
#include "Material/MaterialTemplate.h"
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

        void Clear();

    private:
        struct FEntry {
            Asset::FAssetHandle                Handle{};
            Container::TShared<Render::FMaterial> Material;
        };

        [[nodiscard]] auto FindEntryIndex(const Asset::FAssetHandle& handle) const noexcept -> isize;
        [[nodiscard]] auto CreateMaterialFromAsset(const Asset::FMaterialAsset& asset) const
            -> Container::TShared<Render::FMaterial>;

        Asset::FAssetManager*                     mAssetManager = nullptr;
        Container::TShared<Render::FMaterialTemplate> mDefaultTemplate;
        Render::FMaterial*                        mDefaultMaterial = nullptr;
        Container::TShared<Render::FMaterial>     mFallbackMaterial;
        Container::TVector<FEntry>                mEntries;
    };
} // namespace AltinaEngine::Engine
