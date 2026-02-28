#include "Engine/GameScene/SkyCubeComponent.h"

#include "Engine/GameScene/ComponentRegistry.h"
#include "Engine/GameScene/World.h"

using AltinaEngine::Move;
namespace AltinaEngine::GameScene {
    namespace {
        void SerializeSkyCubeComponent(
            FWorld& world, FComponentId id, Core::Reflection::ISerializer& serializer) {
            auto&      component = world.ResolveComponent<FSkyCubeComponent>(id);
            const auto handle    = component.GetCubeMapAsset();

            serializer.Write(static_cast<u8>(handle.Type));
            const auto& bytes = handle.Uuid.GetBytes();
            for (usize i = 0U; i < AltinaEngine::FUuid::kByteCount; ++i) {
                serializer.Write(bytes[i]);
            }
        }

        void DeserializeSkyCubeComponent(
            FWorld& world, FComponentId id, Core::Reflection::IDeserializer& deserializer) {
            auto&               component = world.ResolveComponent<FSkyCubeComponent>(id);

            Asset::FAssetHandle handle{};
            handle.Type = static_cast<Asset::EAssetType>(deserializer.Read<u8>());

            FUuid::FBytes bytes{};
            for (usize i = 0U; i < FUuid::kByteCount; ++i) {
                bytes[i] = deserializer.Read<u8>();
            }
            handle.Uuid = FUuid(bytes);

            component.SetCubeMapAsset(handle);
        }

        struct FSkyCubeComponentRegistryHook final {
            FSkyCubeComponentRegistryHook() {
                FComponentTypeEntry entry = BuildComponentTypeEntry<FSkyCubeComponent>();
                entry.Serialize           = &SerializeSkyCubeComponent;
                entry.Deserialize         = &DeserializeSkyCubeComponent;
                GetComponentRegistry().Register(entry);
            }
        };

        // Register SkyCubeComponent with custom binary serialization.
        FSkyCubeComponentRegistryHook gSkyCubeComponentRegistryHook{};
    } // namespace

    FSkyCubeComponent::FAssetToSkyCubeConverter FSkyCubeComponent::AssetToSkyCubeConverter = {};

    void FSkyCubeComponent::SetCubeMapAsset(Asset::FAssetHandle handle) noexcept {
        mCubeMapAsset  = Move(handle);
        mResolved      = false;
        mResolvedAsset = {};
        mRhi           = {};
    }

    auto FSkyCubeComponent::GetCubeMapRhi() const noexcept -> const FSkyCubeRhiResources& {
        ResolveSkyCube();
        return mRhi;
    }

    void FSkyCubeComponent::ResolveSkyCube() const noexcept {
        if (mResolvedAsset != mCubeMapAsset) {
            mResolved      = false;
            mResolvedAsset = mCubeMapAsset;
        }

        if (mResolved) {
            return;
        }

        if (!AssetToSkyCubeConverter) {
            return;
        }

        mResolved = true;
        mRhi      = {};

        if (!mCubeMapAsset.IsValid()) {
            return;
        }

        mRhi = AssetToSkyCubeConverter(mCubeMapAsset);
    }
} // namespace AltinaEngine::GameScene
