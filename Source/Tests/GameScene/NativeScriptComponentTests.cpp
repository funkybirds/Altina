#include "TestHarness.h"

#include "Base/AltinaBase.h"
#include "Engine/EngineReflection.h"
#include "Engine/GameScene/ComponentRegistry.h"
#include "Engine/GameScene/NativeScriptComponent.h"
#include "Engine/GameScene/World.h"
#include "Reflection/BinaryDeserializer.h"
#include "Reflection/BinarySerializer.h"
#include "Types/Aliases.h"

using AltinaEngine::i32;
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
} // namespace

TEST_CASE("GameScene.NativeScriptComponent.TickAndSerializationRoundTrip") {
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

    // Ensure the type is registered before deserializing the world.
    RegisterTestNativeScriptComponent();

    auto loaded = FWorld::Deserialize(deserializer);
    REQUIRE(static_cast<bool>(loaded));

    const auto loadedId = loaded->GetComponent<FTestNativeScript>(obj.GetId());
    REQUIRE(loadedId.IsValid());

    auto& loadedScript = loaded->ResolveComponent<FTestNativeScript>(loadedId);
    REQUIRE_EQ(loadedScript.Value, 1234);
    REQUIRE_EQ(loadedScript.OnCreateCount, 1);
    REQUIRE_EQ(loadedScript.OnEnableCount, 1);
    REQUIRE_EQ(loadedScript.TickCount, 0);

    loaded->Tick(0.016f);
    REQUIRE_EQ(loadedScript.TickCount, 1);
}
