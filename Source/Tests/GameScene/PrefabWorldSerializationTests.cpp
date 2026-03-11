#include "TestHarness.h"

#include "Base/AltinaBase.h"
#include "Engine/GameScene/World.h"
#include "Engine/GameSceneAsset/Prefab.h"
#include "Reflection/BinaryDeserializer.h"
#include "Reflection/BinarySerializer.h"
#include "Utility/Uuid.h"

using namespace AltinaEngine;
using namespace AltinaEngine::Core::Reflection;
using namespace AltinaEngine::GameScene;

namespace {
    struct FPrefabTestDataComponent : public FComponent {
        i32 Value = 0;
    };

    void SerializePrefabTestData(FWorld& world, FComponentId id, ISerializer& serializer) {
        auto& component = world.ResolveComponent<FPrefabTestDataComponent>(id);
        serializer.Write(component.Value);
    }

    void DeserializePrefabTestData(FWorld& world, FComponentId id, IDeserializer& deserializer) {
        auto& component = world.ResolveComponent<FPrefabTestDataComponent>(id);
        component.Value = deserializer.Read<i32>();
    }

    void RegisterPrefabTestDataComponent() {
        auto entry        = BuildComponentTypeEntry<FPrefabTestDataComponent>();
        entry.Serialize   = &SerializePrefabTestData;
        entry.Deserialize = &DeserializePrefabTestData;
        GetComponentRegistry().Register(entry);
    }

    auto MakeUuid(u8 seed) -> FUuid {
        FUuid::FBytes bytes{};
        for (u32 i = 0U; i < FUuid::kByteCount; ++i) {
            bytes[i] = static_cast<u8>(seed + static_cast<u8>(i));
        }
        return FUuid(bytes);
    }

    auto ReadSerializedString(FBinaryDeserializer& deserializer) -> Core::Container::FString {
        const u32 length = deserializer.Read<u32>();
        if (length == 0U) {
            return {};
        }

        Core::Container::TVector<TChar> text{};
        text.Resize(length);
        for (u32 i = 0U; i < length; ++i) {
            text[i] = deserializer.Read<TChar>();
        }
        return Core::Container::FString(text.Data(), static_cast<usize>(length));
    }

    auto ReadSerializedNativeString(FBinaryDeserializer& deserializer)
        -> Core::Container::FNativeString {
        const u32 length = deserializer.Read<u32>();
        if (length == 0U) {
            return {};
        }

        Core::Container::TVector<char> text{};
        text.Resize(length);
        for (u32 i = 0U; i < length; ++i) {
            text[i] = deserializer.Read<char>();
        }
        return Core::Container::FNativeString(text.Data(), static_cast<usize>(length));
    }

    void SkipSerializedTransform(FBinaryDeserializer& deserializer) {
        for (u32 i = 0U; i < 10U; ++i) {
            (void)deserializer.Read<f32>();
        }
    }
} // namespace

TEST_CASE("GameScene.World.SerializationV2.PrefabRecordAndSubtreeFiltering") {
    RegisterPrefabTestDataComponent();

    FWorld world(222);

    auto   prefabRoot  = world.CreateGameObject(TEXT("PrefabRoot"));
    auto   prefabChild = world.CreateGameObject(TEXT("PrefabChild"));
    prefabChild.SetParent(prefabRoot.GetId());
    auto rawObject = world.CreateGameObject(TEXT("RawObject"));

    auto prefabRootComponent  = prefabRoot.AddComponent<FPrefabTestDataComponent>();
    auto prefabChildComponent = prefabChild.AddComponent<FPrefabTestDataComponent>();
    auto rawComponent         = rawObject.AddComponent<FPrefabTestDataComponent>();
    REQUIRE(prefabRootComponent.IsValid());
    REQUIRE(prefabChildComponent.IsValid());
    REQUIRE(rawComponent.IsValid());

    prefabRootComponent.Get().Value  = 11;
    prefabChildComponent.Get().Value = 22;
    rawComponent.Get().Value         = 33;

    Engine::GameSceneAsset::FPrefabDescriptor descriptor{};
    descriptor.LoaderType        = "engine.prefab.model_asset";
    descriptor.AssetHandle.mType = Asset::EAssetType::Model;
    descriptor.AssetHandle.mUuid = MakeUuid(5U);
    world.RegisterPrefabRoot(prefabRoot.GetId(), descriptor);

    FBinarySerializer serializer;
    world.Serialize(serializer);

    FBinaryDeserializer deserializer;
    deserializer.SetBuffer(serializer.GetBuffer());

    REQUIRE(deserializer.Read<u32>() == 2U);
    REQUIRE(deserializer.Read<u32>() == world.GetWorldId());
    REQUIRE(deserializer.Read<u32>() == 2U);

    // Prefab root record: only prefab descriptor should be serialized (no component payload).
    REQUIRE(deserializer.Read<u8>() == 1U);
    REQUIRE(deserializer.Read<u32>() == prefabRoot.GetId().Index);
    REQUIRE(deserializer.Read<u32>() == prefabRoot.GetId().Generation);
    REQUIRE(ReadSerializedString(deserializer).ToView() == TEXT("PrefabRoot"));
    REQUIRE(deserializer.Read<bool>() == true);
    REQUIRE(deserializer.Read<bool>() == false);
    SkipSerializedTransform(deserializer);
    REQUIRE(ReadSerializedNativeString(deserializer).ToView() == "engine.prefab.model_asset");
    const auto prefabHandle = Asset::FAssetHandle::Deserialize(deserializer);
    REQUIRE(prefabHandle == descriptor.AssetHandle);

    // Raw record remains unchanged.
    REQUIRE(deserializer.Read<u8>() == 0U);
    REQUIRE(deserializer.Read<u32>() == rawObject.GetId().Index);
    REQUIRE(deserializer.Read<u32>() == rawObject.GetId().Generation);
    REQUIRE(ReadSerializedString(deserializer).ToView() == TEXT("RawObject"));
    REQUIRE(deserializer.Read<bool>() == true);
    REQUIRE(deserializer.Read<bool>() == false);
    SkipSerializedTransform(deserializer);
    REQUIRE(deserializer.Read<u32>() == 1U);
    REQUIRE(deserializer.Read<FComponentTypeHash>()
        == GetComponentTypeHash<FPrefabTestDataComponent>());
    REQUIRE(deserializer.Read<bool>() == true);
    REQUIRE(deserializer.Read<i32>() == 33);

    REQUIRE(!deserializer.HasMoreData());
}
