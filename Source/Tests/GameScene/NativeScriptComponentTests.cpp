#include "TestHarness.h"

#include "Base/AltinaBase.h"
#include "Asset/AssetManager.h"
#include "Engine/EngineReflection.h"
#include "Engine/GameScene/ComponentRegistry.h"
#include "Engine/GameScene/NativeScriptComponent.h"
#include "Engine/GameScene/World.h"
#include "Reflection/BinaryDeserializer.h"
#include "Reflection/BinarySerializer.h"
#include "Types/Aliases.h"

using AltinaEngine::f32;
using AltinaEngine::i32;
using AltinaEngine::TChar;
using AltinaEngine::u32;
using AltinaEngine::u8;
using AltinaEngine::usize;
using namespace AltinaEngine::GameScene;

namespace {
    class FTestNativeScript final : public FNativeScriptComponent {
    public:
        void OnCreate() override { ++OnCreateCount; }
        void OnDestroy() override { ++OnDestroyCount; }
        void OnEnable() override { ++OnEnableCount; }
        void OnDisable() override { ++OnDisableCount; }
        void Tick(float /*dt*/) override { ++TickCount; }

        i32  Value          = 0;
        i32  OnCreateCount  = 0;
        i32  OnDestroyCount = 0;
        i32  OnEnableCount  = 0;
        i32  OnDisableCount = 0;
        i32  TickCount      = 0;
    };

    void SerializeTestNativeScript(
        FWorld& world, FComponentId id, AltinaEngine::Core::Reflection::ISerializer& serializer) {
        auto& component = world.ResolveComponent<FTestNativeScript>(id);
        serializer.Write(component.Value);
    }

    void DeserializeTestNativeScript(FWorld& world, FComponentId id,
        AltinaEngine::Core::Reflection::IDeserializer& deserializer) {
        auto& component = world.ResolveComponent<FTestNativeScript>(id);
        component.Value = deserializer.Read<i32>();
    }

    void RegisterTestNativeScriptComponent() {
        auto entry        = BuildComponentTypeEntry<FTestNativeScript>();
        entry.Serialize   = &SerializeTestNativeScript;
        entry.Deserialize = &DeserializeTestNativeScript;
        GetComponentRegistry().Register(entry);
    }

    auto ReadSerializedString(AltinaEngine::Core::Reflection::FBinaryDeserializer& deserializer)
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
        return AltinaEngine::Core::Container::FString(text.Data(), static_cast<usize>(length));
    }
} // namespace

TEST_CASE("GameScene.NativeScriptComponent.TickAndSerializationV2Raw") {
    AltinaEngine::Engine::RegisterEngineReflection();
    RegisterTestNativeScriptComponent();

    FWorld     world(77);
    auto       obj = world.CreateGameObject(TEXT("NativeScriptOwner"));

    const auto id = world.CreateComponent<FTestNativeScript>(
        obj.GetId(), [](FTestNativeScript& c) { c.Value = 1234; });
    REQUIRE(id.IsValid());

    auto& script = world.ResolveComponent<FTestNativeScript>(id);
    REQUIRE_EQ(script.Value, 1234);
    REQUIRE_EQ(script.OnCreateCount, 1);
    REQUIRE_EQ(script.OnEnableCount, 1);
    REQUIRE_EQ(script.TickCount, 0);

    world.Tick(0.016f);
    REQUIRE_EQ(script.TickCount, 1);

    AltinaEngine::Core::Reflection::FBinarySerializer serializer;
    world.Serialize(serializer);

    AltinaEngine::Core::Reflection::FBinaryDeserializer deserializer;
    deserializer.SetBuffer(serializer.GetBuffer());

    REQUIRE(deserializer.Read<u32>() == 2U);
    REQUIRE(deserializer.Read<u32>() == world.GetWorldId());
    REQUIRE(deserializer.Read<u32>() == 1U);

    REQUIRE(deserializer.Read<u8>() == 0U);
    REQUIRE(deserializer.Read<u32>() == obj.GetId().Index);
    REQUIRE(deserializer.Read<u32>() == obj.GetId().Generation);
    REQUIRE(ReadSerializedString(deserializer).ToView() == TEXT("NativeScriptOwner"));
    REQUIRE(deserializer.Read<bool>() == true);
    REQUIRE(deserializer.Read<bool>() == false);

    for (u32 i = 0U; i < 10U; ++i) {
        (void)deserializer.Read<f32>();
    }

    REQUIRE(deserializer.Read<u32>() == 1U);
    REQUIRE(deserializer.Read<FComponentTypeHash>() == GetComponentTypeHash<FTestNativeScript>());
    REQUIRE(deserializer.Read<bool>() == true);
    REQUIRE(deserializer.Read<i32>() == 1234);
}

TEST_CASE("GameScene.WorldDeserializeEditorRestoreSkipsComponentLifecycles") {
    AltinaEngine::Engine::RegisterEngineReflection();
    RegisterTestNativeScriptComponent();

    FWorld     world(91);
    auto       obj = world.CreateGameObject(TEXT("EditorRestoreOwner"));
    const auto id  = world.CreateComponent<FTestNativeScript>(
        obj.GetId(), [](FTestNativeScript& c) { c.Value = 77; });
    REQUIRE(id.IsValid());

    AltinaEngine::Core::Reflection::FBinarySerializer serializer;
    world.Serialize(serializer);

    AltinaEngine::Core::Reflection::FBinaryDeserializer deserializer;
    deserializer.SetBuffer(serializer.GetBuffer());

    AltinaEngine::Asset::FAssetManager assetManager{};
    const auto                         restored =
        FWorld::Deserialize(deserializer, assetManager, EWorldDeserializeMode::EditorRestore);
    REQUIRE(restored);

    const auto restoredObjects = restored->GetAllGameObjectIds();
    REQUIRE_EQ(restoredObjects.Size(), 1U);
    const auto restoredComponentId = restored->GetComponent<FTestNativeScript>(restoredObjects[0]);
    REQUIRE(restoredComponentId.IsValid());

    auto& restoredScript = restored->ResolveComponent<FTestNativeScript>(restoredComponentId);
    REQUIRE_EQ(restoredScript.Value, 77);
    REQUIRE_EQ(restoredScript.OnCreateCount, 0);
    REQUIRE_EQ(restoredScript.OnEnableCount, 0);
    REQUIRE(restoredScript.IsEnabled());
}
