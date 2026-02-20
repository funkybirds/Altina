#pragma once

#include "Engine/EngineAPI.h"
#include "Engine/GameScene/Component.h"
#include "Container/SmartPtr.h"
#include "Container/Vector.h"
#include "Material/Material.h"
#include "Types/Traits.h"

namespace AltinaEngine::GameScene {
    namespace Container = Core::Container;
    using Container::TShared;
    using Container::TVector;
    using RenderCore::FMaterial;

    /**
     * @brief Component holding mesh material bindings.
     */
    class AE_ENGINE_API FMeshMaterialComponent : public FComponent {
    public:
        [[nodiscard]] auto GetMaterials() noexcept -> TVector<TShared<FMaterial>>& {
            return mMaterials;
        }
        [[nodiscard]] auto GetMaterials() const noexcept -> const TVector<TShared<FMaterial>>& {
            return mMaterials;
        }

        void SetMaterials(TVector<TShared<FMaterial>>&& materials) noexcept {
            mMaterials = Move(materials);
        }
        void               ClearMaterials() { mMaterials.Clear(); }

        [[nodiscard]] auto GetMaterialCount() const noexcept -> u32 {
            return static_cast<u32>(mMaterials.Size());
        }
        [[nodiscard]] auto GetMaterial(u32 slot) const noexcept -> const FMaterial* {
            if (slot >= mMaterials.Size()) {
                return nullptr;
            }
            return mMaterials[slot].Get();
        }
        void SetMaterial(u32 slot, TShared<FMaterial> material) {
            if (slot >= mMaterials.Size()) {
                mMaterials.Resize(static_cast<usize>(slot) + 1U);
            }
            mMaterials[slot] = Move(material);
        }

    private:
        TVector<TShared<FMaterial>> mMaterials{};
    };
} // namespace AltinaEngine::GameScene
