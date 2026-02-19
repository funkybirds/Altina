#pragma once

#include "Engine/EngineAPI.h"
#include "Engine/GameScene/Component.h"
#include "Asset/AssetTypes.h"
#include "Types/Traits.h"

namespace AltinaEngine::GameScene {
    namespace Asset     = AltinaEngine::Asset;
    namespace Container = Core::Container;
    using Container::TVector;

    /**
     * @brief Component holding mesh material bindings.
     */
    class AE_ENGINE_API FMeshMaterialComponent : public FComponent {
    public:
        [[nodiscard]] auto GetMaterials() noexcept -> TVector<Asset::FAssetHandle>& {
            return mMaterials;
        }
        [[nodiscard]] auto GetMaterials() const noexcept -> const TVector<Asset::FAssetHandle>& {
            return mMaterials;
        }

        void SetMaterials(TVector<Asset::FAssetHandle>&& materials) noexcept {
            mMaterials = Move(materials);
        }
        void               ClearMaterials() { mMaterials.Clear(); }

        [[nodiscard]] auto GetMaterialCount() const noexcept -> u32 {
            return static_cast<u32>(mMaterials.Size());
        }
        [[nodiscard]] auto GetMaterial(u32 slot) const noexcept -> const Asset::FAssetHandle* {
            if (slot >= mMaterials.Size()) {
                return nullptr;
            }
            return &mMaterials[slot];
        }
        void SetMaterial(u32 slot, Asset::FAssetHandle material) {
            if (slot >= mMaterials.Size()) {
                mMaterials.Resize(static_cast<usize>(slot) + 1U);
            }
            mMaterials[slot] = material;
        }

    private:
        TVector<Asset::FAssetHandle> mMaterials{};
    };
} // namespace AltinaEngine::GameScene
