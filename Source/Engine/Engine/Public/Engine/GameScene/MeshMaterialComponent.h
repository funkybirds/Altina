#pragma once

#include "Engine/EngineAPI.h"
#include "Engine/GameScene/Component.h"
#include "Asset/AssetTypes.h"
#include "Asset/MeshMaterialParameterBlock.h"
#include "Container/Function.h"
#include "Container/Vector.h"
#include "Types/Traits.h"
#include "Reflection/ReflectionAnnotations.h"
#include "Reflection/ReflectionFwd.h"

namespace AltinaEngine::RenderCore {
    class FMaterial;
} // namespace AltinaEngine::RenderCore

namespace AltinaEngine::GameScene {
    using AltinaEngine::Move;
    namespace Container = Core::Container;
    using Container::TFunction;
    using Container::TVector;
    namespace Asset  = AltinaEngine::Asset;
    namespace Render = RenderCore;
    /**
     * @brief Component holding mesh material bindings.
     */
    class ACLASS() AE_ENGINE_API FMeshMaterialComponent : public FComponent {
    public:
        struct FMaterialSlot {
            Asset::FAssetHandle                Template;
            Asset::FMeshMaterialParameterBlock Parameters;
        };

        using FAssetToRenderMaterialConverter = TFunction<Render::FMaterial(
            const Asset::FAssetHandle&, const Asset::FMeshMaterialParameterBlock&)>;

        [[nodiscard]] auto GetMaterials() noexcept -> TVector<FMaterialSlot>& { return mMaterials; }
        [[nodiscard]] auto GetMaterials() const noexcept -> const TVector<FMaterialSlot>& {
            return mMaterials;
        }

        void SetMaterials(TVector<FMaterialSlot>&& materials) noexcept {
            mMaterials = Move(materials);
        }
        void               ClearMaterials() { mMaterials.Clear(); }

        [[nodiscard]] auto GetMaterialCount() const noexcept -> u32 {
            return static_cast<u32>(mMaterials.Size());
        }

        [[nodiscard]] auto GetMaterialSlot(u32 slot) noexcept -> FMaterialSlot* {
            if (slot >= mMaterials.Size()) {
                return nullptr;
            }
            return &mMaterials[slot];
        }
        [[nodiscard]] auto GetMaterialSlot(u32 slot) const noexcept -> const FMaterialSlot* {
            if (slot >= mMaterials.Size()) {
                return nullptr;
            }
            return &mMaterials[slot];
        }

        void SetMaterialTemplate(u32 slot, Asset::FAssetHandle handle) {
            if (slot >= mMaterials.Size()) {
                mMaterials.Resize(static_cast<usize>(slot) + 1U);
            }
            mMaterials[slot].Template = Move(handle);
        }

        void SetMaterialParameters(u32 slot, Asset::FMeshMaterialParameterBlock parameters) {
            if (slot >= mMaterials.Size()) {
                mMaterials.Resize(static_cast<usize>(slot) + 1U);
            }
            mMaterials[slot].Parameters = Move(parameters);
        }

        void SetMaterialSlot(
            u32 slot, Asset::FAssetHandle handle, Asset::FMeshMaterialParameterBlock parameters) {
            if (slot >= mMaterials.Size()) {
                mMaterials.Resize(static_cast<usize>(slot) + 1U);
            }
            mMaterials[slot].Template   = Move(handle);
            mMaterials[slot].Parameters = Move(parameters);
        }

        [[nodiscard]] auto GetRenderMaterialData(u32 slot) const -> Render::FMaterial;

        static FAssetToRenderMaterialConverter AssetToRenderMaterialConverter;

    private:
        template <auto Member>
        friend struct AltinaEngine::Core::Reflection::Detail::TAutoMemberAccessor;

    public:
        APROPERTY()
        TVector<FMaterialSlot> mMaterials{};
    };
} // namespace AltinaEngine::GameScene
