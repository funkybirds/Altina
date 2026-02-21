#include "Engine/GameScene/MeshMaterialComponent.h"

#include "Material/Material.h"

namespace AltinaEngine::GameScene {
    FMeshMaterialComponent::FAssetToRenderMaterialConverter
         FMeshMaterialComponent::AssetToRenderMaterialConverter = {};

    auto FMeshMaterialComponent::GetRenderMaterialData(u32 slot) const -> Render::FMaterial {
        const auto* entry = GetMaterialSlot(slot);
        if (entry == nullptr || !AssetToRenderMaterialConverter) {
            return {};
        }
        return AssetToRenderMaterialConverter(entry->Template, entry->Parameters);
    }
} // namespace AltinaEngine::GameScene
