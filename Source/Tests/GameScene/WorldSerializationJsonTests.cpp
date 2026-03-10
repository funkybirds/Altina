#include "TestHarness.h"

#include "Base/AltinaBase.h"
#include "Engine/GameScene/ComponentRegistry.h"
#include "Engine/GameScene/CameraComponent.h"
#include "Engine/GameScene/DirectionalLightComponent.h"
#include "Engine/GameScene/PointLightComponent.h"
#include "Engine/GameScene/World.h"
#include "Reflection/JsonSerializer.h"

using namespace AltinaEngine;
using namespace AltinaEngine::Core::Reflection;
using namespace AltinaEngine::GameScene;

namespace {
    struct FJsonRegistryComponent : public FComponent {
        i32 Value = 0;
    };

    void SerializeJsonRegistryComponent(
        FWorld& world, FComponentId id, Core::Reflection::ISerializer& serializer) {
        const auto& component = world.ResolveComponent<FJsonRegistryComponent>(id);
        serializer.BeginObject({});
        serializer.WriteFieldName(TEXT("jsonValue"));
        serializer.Write(component.Value);
        serializer.EndObject();
    }

    void SerializeRegistryComponent(
        FWorld& world, FComponentId id, Core::Reflection::ISerializer& serializer) {
        const auto& component = world.ResolveComponent<FJsonRegistryComponent>(id);
        serializer.Write(component.Value);
    }

    void DeserializeRegistryComponent(
        FWorld& world, FComponentId id, Core::Reflection::IDeserializer& deserializer) {
        auto& component = world.ResolveComponent<FJsonRegistryComponent>(id);
        component.Value = deserializer.Read<i32>();
    }

    void RegisterJsonRegistryComponent() {
        static bool registered = false;
        if (registered) {
            return;
        }
        registered = true;

        auto entry          = BuildComponentTypeEntry<FJsonRegistryComponent>();
        entry.Serialize     = &SerializeRegistryComponent;
        entry.SerializeJson = &SerializeJsonRegistryComponent;
        entry.Deserialize   = &DeserializeRegistryComponent;
        GetComponentRegistry().Register(entry);
    }
} // namespace

TEST_CASE("GameScene.World.SerializationV2.Json.RegistrySerializer") {
    RegisterJsonRegistryComponent();

    FWorld world(301);
    auto   object = world.CreateGameObject(TEXT("JsonRegistry"));
    auto   id     = world.CreateComponent<FJsonRegistryComponent>(object.GetId());
    REQUIRE(id.IsValid());

    world.ResolveComponent<FJsonRegistryComponent>(id).Value = 77;

    FJsonSerializer serializer;
    world.SerializeJson(serializer);

    const auto text = serializer.GetString();
    REQUIRE(text.Contains("\"jsonValue\":77"));
    REQUIRE(!text.Contains("UnknownComponent"));
}

TEST_CASE("GameScene.World.SerializationV2.Json.BuiltinComponentNames") {
    FWorld     world(302);
    auto       object = world.CreateGameObject(TEXT("Lights"));

    const auto camera      = world.CreateComponent<FCameraComponent>(object.GetId());
    const auto directional = world.CreateComponent<FDirectionalLightComponent>(object.GetId());
    const auto point       = world.CreateComponent<FPointLightComponent>(object.GetId());
    REQUIRE(camera.IsValid());
    REQUIRE(directional.IsValid());
    REQUIRE(point.IsValid());

    FJsonSerializer serializer;
    world.SerializeJson(serializer);

    const auto text = serializer.GetString();
    REQUIRE(text.Contains("FDirectionalLightComponent"));
    REQUIRE(text.Contains("FPointLightComponent"));
    REQUIRE(text.Contains("fovYRadians"));
    REQUIRE(text.Contains("castShadows"));
    REQUIRE(text.Contains("range"));
    REQUIRE(!text.Contains("UnknownComponent"));
}
