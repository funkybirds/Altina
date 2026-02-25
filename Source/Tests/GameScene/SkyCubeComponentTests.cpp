#include "TestHarness.h"

#include "Base/AltinaBase.h"
#include "Engine/GameScene/SkyCubeComponent.h"
#include "Engine/GameScene/World.h"
#include "Reflection/BinaryDeserializer.h"
#include "Reflection/BinarySerializer.h"
#include "Utility/Uuid.h"

#include <array>

namespace {
    using AltinaEngine::u32;
    using AltinaEngine::u8;
    using AltinaEngine::Asset::EAssetType;
    using AltinaEngine::Asset::FAssetHandle;
    using AltinaEngine::Core::Reflection::FBinaryDeserializer;
    using AltinaEngine::Core::Reflection::FBinarySerializer;
    using AltinaEngine::Core::Utility::FUuid;
    using AltinaEngine::GameScene::FComponentId;
    using AltinaEngine::GameScene::FSkyCubeComponent;
    using AltinaEngine::GameScene::FWorld;

    auto MakeUuid(u8 seed) -> FUuid {
        std::array<u8, FUuid::kByteCount> bytes{};
        for (u32 i = 0U; i < FUuid::kByteCount; ++i) {
            bytes[i] = static_cast<u8>(seed + static_cast<u8>(i));
        }
        return FUuid(bytes);
    }

    template <typename TList> auto Contains(const TList& list, FComponentId id) -> bool {
        for (const auto& entry : list) {
            if (entry == id) {
                return true;
            }
        }
        return false;
    }
} // namespace

TEST_CASE("GameScene.SkyCubeComponent.ActiveList") {
    FWorld world;
    auto   obj = world.CreateGameObject(TEXT("Sky"));
    REQUIRE(obj.IsValid());

    auto comp = obj.AddComponent<FSkyCubeComponent>();
    REQUIRE(comp.IsValid());

    const FComponentId id = comp.GetId();
    REQUIRE(Contains(world.GetActiveSkyCubeComponents(), id));

    comp.Get().SetEnabled(false);
    REQUIRE(!Contains(world.GetActiveSkyCubeComponents(), id));

    comp.Get().SetEnabled(true);
    REQUIRE(Contains(world.GetActiveSkyCubeComponents(), id));

    obj.SetActive(false);
    REQUIRE(!Contains(world.GetActiveSkyCubeComponents(), id));

    obj.SetActive(true);
    REQUIRE(Contains(world.GetActiveSkyCubeComponents(), id));
}

TEST_CASE("GameScene.SkyCubeComponent.Serialization.BinaryRoundTrip") {
    FWorld world(7);
    auto   obj = world.CreateGameObject(TEXT("Sky"));
    REQUIRE(obj.IsValid());

    auto compRef = obj.AddComponent<FSkyCubeComponent>();
    REQUIRE(compRef.IsValid());

    FAssetHandle handle{};
    handle.Uuid = MakeUuid(42U);
    handle.Type = EAssetType::Texture2D;

    compRef.Get().SetCubeMapAsset(handle);

    FBinarySerializer serializer;
    world.Serialize(serializer);

    FBinaryDeserializer deserializer;
    deserializer.SetBuffer(serializer.GetBuffer());
    auto loaded = FWorld::Deserialize(deserializer);
    REQUIRE(static_cast<bool>(loaded));

    const auto loadedCompId = loaded->GetComponent<FSkyCubeComponent>(obj.GetId());
    REQUIRE(loadedCompId.IsValid());

    const auto& loadedComp = loaded->ResolveComponent<FSkyCubeComponent>(loadedCompId);
    REQUIRE(loadedComp.GetCubeMapAsset() == handle);
}
