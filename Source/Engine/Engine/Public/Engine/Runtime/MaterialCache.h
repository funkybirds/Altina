#pragma once

#include "Engine/EngineAPI.h"

#include "Asset/AssetTypes.h"
#include "Asset/MeshMaterialParameterBlock.h"
#include "Container/SmartPtr.h"
#include "Material/Material.h"
#include "Material/MaterialTemplate.h"
#include "Container/HashMap.h"
#include "Types/Aliases.h"
#include "Types/Traits.h"

namespace AltinaEngine::Engine {
    using AltinaEngine::Move;
    namespace Container = Core::Container;
    namespace Render    = AltinaEngine::RenderCore;
    namespace Asset     = AltinaEngine::Asset;

    /**
     * @brief Cache mapping material assets to render-core materials.
     */
    class AE_ENGINE_API FMaterialCache {
    public:
        void SetDefaultMaterial(Render::FMaterial* material) noexcept {
            mDefaultMaterial = material;
        }
        void SetDefaultTemplate(Container::TShared<Render::FMaterialTemplate> templ) noexcept {
            mDefaultTemplate = Move(templ);
        }

        [[nodiscard]] auto ResolveDefault() -> Render::FMaterial*;
        [[nodiscard]] auto ResolveMaterial(const Asset::FAssetHandle& handle,
            const Asset::FMeshMaterialParameterBlock& parameters) -> Render::FMaterial*;
        void               PrepareMaterialForRendering(Render::FMaterial& material);

        void               Clear();

    private:
        struct FMaterialCacheKey {
            Asset::FAssetHandle Handle{};
            u64                 ParamHash = 0ULL;

            [[nodiscard]] auto  operator==(const FMaterialCacheKey& other) const noexcept -> bool {
                return Handle == other.Handle && ParamHash == other.ParamHash;
            }
        };

        struct FMaterialCacheKeyHash {
            auto operator()(const FMaterialCacheKey& key) const noexcept -> usize {
                constexpr u64 kFnvOffset64 = 1469598103934665603ULL;
                constexpr u64 kFnvPrime64  = 1099511628211ULL;

                u64           hash  = kFnvOffset64;
                const auto*   bytes = key.Handle.Uuid.Data();
                for (usize i = 0U; i < FUuid::kByteCount; ++i) {
                    hash ^= bytes[i];
                    hash *= kFnvPrime64;
                }
                hash ^= static_cast<u8>(key.Handle.Type);
                hash *= kFnvPrime64;
                hash ^= key.ParamHash;
                hash *= kFnvPrime64;
                return static_cast<usize>(hash);
            }
        };

        Container::TShared<Render::FMaterialTemplate> mDefaultTemplate;
        Render::FMaterial*                            mDefaultMaterial = nullptr;
        Container::TShared<Render::FMaterial>         mFallbackMaterial;
        Container::THashMap<FMaterialCacheKey, Container::TShared<Render::FMaterial>,
            FMaterialCacheKeyHash>
            mMaterialCache;
    };
} // namespace AltinaEngine::Engine
