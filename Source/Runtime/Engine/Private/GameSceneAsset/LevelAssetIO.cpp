#include "Engine/GameSceneAsset/LevelAssetIO.h"

#include "Asset/AssetBinary.h"
#include "Asset/AssetManager.h"
#include "Asset/LevelAsset.h"
#include "Engine/GameScene/World.h"
#include "Reflection/BinaryDeserializer.h"
#include "Reflection/JsonSerializer.h"
#include "Types/CheckedCast.h"

namespace AltinaEngine::Engine::GameSceneAsset {
    auto SaveWorldAsLevelJson(
        const GameScene::FWorld& world, Core::Container::FNativeString& outJson) -> bool {
        Core::Reflection::FJsonSerializer serializer{};
        world.SerializeJson(serializer);
        outJson = serializer.GetString();
        return !outJson.IsEmptyString();
    }

    auto LoadWorldFromLevelAsset(const Asset::FAssetHandle& levelHandle,
        Asset::FAssetManager& manager, GameScene::EWorldDeserializeMode mode)
        -> Core::Container::TOwner<GameScene::FWorld> {
        const auto asset = manager.Load(levelHandle);
        if (!asset) {
            return {};
        }

        const auto* levelAsset = AltinaEngine::CheckedCast<const Asset::FLevelAsset*>(asset.Get());
        if (levelAsset == nullptr) {
            return {};
        }

        const auto& payload = levelAsset->GetPayload();
        if (levelAsset->GetEncoding() == Asset::kLevelEncodingWorldBinary) {
            Core::Reflection::FBinaryDeserializer deserializer{};
            deserializer.SetBuffer(payload);
            return GameScene::FWorld::Deserialize(deserializer, manager, mode);
        }

        if (levelAsset->GetEncoding() == Asset::kLevelEncodingWorldJson) {
            Core::Container::FNativeString jsonText{};
            if (!payload.IsEmpty()) {
                jsonText.Append(reinterpret_cast<const char*>(payload.Data()), payload.Size());
            }
            return GameScene::FWorld::DeserializeJson(jsonText.ToView(), manager, mode);
        }

        return {};
    }
} // namespace AltinaEngine::Engine::GameSceneAsset
