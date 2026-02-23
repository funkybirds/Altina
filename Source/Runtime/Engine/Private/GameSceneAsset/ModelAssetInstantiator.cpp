#include "Engine/GameSceneAsset/ModelAssetInstantiator.h"

#include "Asset/ModelAsset.h"
#include "Asset/ModelAssetLoader.h"
#include "Engine/GameScene/MeshMaterialComponent.h"
#include "Engine/GameScene/StaticMeshFilterComponent.h"
#include "Engine/GameScene/World.h"
#include "Math/LinAlg/SpatialTransform.h"

namespace AltinaEngine::Engine::GameSceneAsset {
    namespace LinAlg = Core::Math::LinAlg;

    auto FModelAssetInstantiator::Instantiate(GameScene::FWorld& world,
        Asset::FAssetManager& manager, const Asset::FAssetHandle& modelHandle)
        -> FModelInstantiateResult {
        FModelInstantiateResult  result{};

        Asset::FModelAssetLoader loader(manager);
        auto                     load = loader.Load(modelHandle);
        if (load.Model == nullptr) {
            return result;
        }

        const auto& nodes     = load.Model->GetNodes();
        const auto& meshRefs  = load.Model->GetMeshRefs();
        const auto& materials = load.Model->GetMaterialSlots();

        result.Nodes.Resize(nodes.Size());
        if (nodes.IsEmpty()) {
            return result;
        }

        for (usize i = 0; i < nodes.Size(); ++i) {
            auto view = world.CreateGameObject((i == 0) ? TEXT("ModelRoot") : TEXT("ModelNode"));
            result.Nodes[i] = view.GetId();

            const auto&               nodeDesc = nodes[i];
            LinAlg::FSpatialTransform transform{};
            transform.Translation = Core::Math::FVector3f(
                nodeDesc.Translation[0], nodeDesc.Translation[1], nodeDesc.Translation[2]);
            transform.Rotation = Core::Math::FQuaternion(nodeDesc.Rotation[0], nodeDesc.Rotation[1],
                nodeDesc.Rotation[2], nodeDesc.Rotation[3]);
            transform.Scale =
                Core::Math::FVector3f(nodeDesc.Scale[0], nodeDesc.Scale[1], nodeDesc.Scale[2]);
            view.SetLocalTransform(transform);
        }

        for (usize i = 0; i < nodes.Size(); ++i) {
            const auto& nodeDesc = nodes[i];
            if (nodeDesc.ParentIndex >= 0
                && static_cast<usize>(nodeDesc.ParentIndex) < result.Nodes.Size()) {
                world.Object(result.Nodes[i]).SetParent(result.Nodes[nodeDesc.ParentIndex]);
            }

            if (nodeDesc.MeshRefIndex < 0
                || static_cast<usize>(nodeDesc.MeshRefIndex) >= meshRefs.Size()) {
                continue;
            }

            auto view              = world.Object(result.Nodes[i]);
            auto meshComponent     = view.AddComponent<GameScene::FStaticMeshFilterComponent>();
            auto materialComponent = view.AddComponent<GameScene::FMeshMaterialComponent>();
            if (meshComponent.IsValid()) {
                meshComponent.Get().SetStaticMeshAsset(meshRefs[nodeDesc.MeshRefIndex].Mesh);
            }
            if (materialComponent.IsValid()) {
                const auto& meshRef = meshRefs[nodeDesc.MeshRefIndex];
                for (u32 slot = 0; slot < meshRef.MaterialSlotCount; ++slot) {
                    const u32 index = meshRef.MaterialSlotOffset + slot;
                    if (index < materials.Size()) {
                        materialComponent.Get().SetMaterialTemplate(slot, materials[index]);
                    }
                }
            }
        }

        result.Root = result.Nodes[0];
        return result;
    }
} // namespace AltinaEngine::Engine::GameSceneAsset
