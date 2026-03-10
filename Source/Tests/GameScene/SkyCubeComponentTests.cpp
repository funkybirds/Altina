#include "TestHarness.h"

#include "Base/AltinaBase.h"
#include "Engine/GameScene/SkyCubeComponent.h"
#include "Engine/GameScene/World.h"
#include "Engine/EngineReflection.h"
#include "Reflection/BinaryDeserializer.h"
#include "Reflection/BinarySerializer.h"
#include "Types/Aliases.h"
#include "Utility/Uuid.h"

namespace {
    using AltinaEngine::f32;
    using AltinaEngine::FUuid;
    using AltinaEngine::TChar;
    using AltinaEngine::u32;
    using AltinaEngine::u8;
    using AltinaEngine::Asset::EAssetType;
    using AltinaEngine::Asset::FAssetHandle;
    using AltinaEngine::Core::Reflection::FBinaryDeserializer;
    using AltinaEngine::Core::Reflection::FBinarySerializer;
    using AltinaEngine::GameScene::FComponentId;
    using AltinaEngine::GameScene::FSkyCubeComponent;
    using AltinaEngine::GameScene::FWorld;

    auto MakeUuid(u8 seed) -> FUuid {
        FUuid::FBytes bytes{};
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

    auto ReadSerializedString(FBinaryDeserializer& deserializer)
        -> AltinaEngine::Core::Container::FString {
        const u32 length = deserializer.Read<u32>();
        if (length == 0U) {
            return {};
        }

        AltinaEngine::Core::Container::TVector<TChar> text{};
        text.Resize(length);
        for (u32 i = 0U; i < length; ++i) {
            text[i] = deserializer.Read<TChar>();
        }
        return AltinaEngine::Core::Container::FString(
            text.Data(), static_cast<AltinaEngine::usize>(length));
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

TEST_CASE("GameScene.SkyCubeComponent.Serialization.BinaryV2Raw") {
    AltinaEngine::Engine::RegisterEngineReflection();

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

    REQUIRE(deserializer.Read<u32>() == 2U);
    REQUIRE(deserializer.Read<u32>() == world.GetWorldId());
    REQUIRE(deserializer.Read<u32>() == 1U);

    REQUIRE(deserializer.Read<u8>() == 0U);
    REQUIRE(deserializer.Read<u32>() == obj.GetId().Index);
    REQUIRE(deserializer.Read<u32>() == obj.GetId().Generation);
    REQUIRE(ReadSerializedString(deserializer).ToView() == TEXT("Sky"));
    REQUIRE(deserializer.Read<bool>() == true);
    REQUIRE(deserializer.Read<bool>() == false);

    for (u32 i = 0U; i < 10U; ++i) {
        (void)deserializer.Read<f32>();
    }

    REQUIRE(deserializer.Read<u32>() == 1U);
    REQUIRE(deserializer.Read<AltinaEngine::GameScene::FComponentTypeHash>()
        == AltinaEngine::GameScene::GetComponentTypeHash<FSkyCubeComponent>());
    REQUIRE(deserializer.Read<bool>() == true);

    const auto serializedType = static_cast<EAssetType>(deserializer.Read<u8>());
    REQUIRE(serializedType == handle.Type);
    FUuid::FBytes bytes{};
    for (u32 i = 0U; i < FUuid::kByteCount; ++i) {
        bytes[i] = deserializer.Read<u8>();
    }
    REQUIRE(FUuid(bytes) == handle.Uuid);
}
