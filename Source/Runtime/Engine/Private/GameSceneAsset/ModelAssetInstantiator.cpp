#include "Engine/GameSceneAsset/ModelAssetInstantiator.h"

#include "Asset/ModelAsset.h"
#include "Asset/ModelAssetLoader.h"
#include "Engine/GameScene/MeshMaterialComponent.h"
#include "Engine/GameScene/StaticMeshFilterComponent.h"
#include "Engine/GameScene/World.h"
#include "Math/LinAlg/SpatialTransform.h"

namespace AltinaEngine::Engine::GameSceneAsset {
    namespace LinAlg = Core::Math::LinAlg;
    using Container::FNativeString;

    namespace {
        FModelAssetInstantiator gModelAssetInstantiator{};

        struct FModelAssetInstantiatorRegistryHook final {
            FModelAssetInstantiatorRegistryHook() {
                GetPrefabInstantiatorRegistry().Register(gModelAssetInstantiator);
            }
        };

        FModelAssetInstantiatorRegistryHook gModelAssetInstantiatorRegistryHook{};
    } // namespace

    FModelAssetInstantiator::FModelAssetInstantiator() : FBasePrefabInstantiator(kLoaderType) {}

    auto FModelAssetInstantiator::Instantiate(GameScene::FWorld& world,
        Asset::FAssetManager& manager, const Asset::FAssetHandle& modelHandle)
        -> FPrefabInstantiateResult {
        FPrefabInstantiateResult result{};

        Asset::FModelAssetLoader loader(manager);
        auto                     load = loader.Load(modelHandle);
        if (load.mModel == nullptr) {
            return result;
        }

        const auto& nodes     = load.mModel->GetNodes();
        const auto& meshRefs  = load.mModel->GetMeshRefs();
        const auto& materials = load.mModel->GetMaterialSlots();

        result.SpawnedNodes.Resize(nodes.Size());
        if (nodes.IsEmpty()) {
            return result;
        }

        for (usize i = 0; i < nodes.Size(); ++i) {
            auto view = world.CreateGameObject((i == 0) ? TEXT("ModelRoot") : TEXT("ModelNode"));
            result.SpawnedNodes[i] = view.GetId();

            const auto&               nodeDesc = nodes[i];
            LinAlg::FSpatialTransform transform{};
            transform.Translation = Core::Math::FVector3f(
                nodeDesc.mTranslation[0], nodeDesc.mTranslation[1], nodeDesc.mTranslation[2]);
            transform.Rotation = Core::Math::FQuaternion(nodeDesc.mRotation[0],
                nodeDesc.mRotation[1], nodeDesc.mRotation[2], nodeDesc.mRotation[3]);
            transform.Scale =
                Core::Math::FVector3f(nodeDesc.mScale[0], nodeDesc.mScale[1], nodeDesc.mScale[2]);
            view.SetLocalTransform(transform);
        }

        for (usize i = 0; i < nodes.Size(); ++i) {
            const auto& nodeDesc = nodes[i];
            if (nodeDesc.mParentIndex >= 0
                && static_cast<usize>(nodeDesc.mParentIndex) < result.SpawnedNodes.Size()) {
                world.Object(result.SpawnedNodes[i])
                    .SetParent(result.SpawnedNodes[nodeDesc.mParentIndex]);
            }

            if (nodeDesc.mMeshRefIndex < 0
                || static_cast<usize>(nodeDesc.mMeshRefIndex) >= meshRefs.Size()) {
                continue;
            }

            auto view              = world.Object(result.SpawnedNodes[i]);
            auto meshComponent     = view.AddComponent<GameScene::FStaticMeshFilterComponent>();
            auto materialComponent = view.AddComponent<GameScene::FMeshMaterialComponent>();
            if (meshComponent.IsValid()) {
                meshComponent.Get().SetStaticMeshAsset(meshRefs[nodeDesc.mMeshRefIndex].mMesh);
            }
            if (materialComponent.IsValid()) {
                const auto& meshRef = meshRefs[nodeDesc.mMeshRefIndex];
                for (u32 slot = 0; slot < meshRef.mMaterialSlotCount; ++slot) {
                    const u32 index = meshRef.mMaterialSlotOffset + slot;
                    if (index < materials.Size()) {
                        materialComponent.Get().SetMaterialTemplate(slot, materials[index]);
                    }
                }
            }
        }

        result.Root = result.SpawnedNodes[0];
        if (result.Root.IsValid()) {
            FPrefabDescriptor descriptor{};
            descriptor.LoaderType  = FNativeString(kLoaderType);
            descriptor.AssetHandle = modelHandle;
            world.RegisterPrefabRoot(result.Root, descriptor);
        }
        return result;
    }
} // namespace AltinaEngine::Engine::GameSceneAsset
