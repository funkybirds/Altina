#include "TestHarness.h"
#include "Base/AltinaBase.h"
#include "Engine/GameScene/World.h"
#include "Reflection/BinaryDeserializer.h"
#include "Reflection/BinarySerializer.h"

using namespace AltinaEngine;
using namespace AltinaEngine::Core;
using namespace AltinaEngine::Core::Reflection;
using namespace AltinaEngine::GameScene;

namespace {
    struct FTestDataComponent : public FComponent {
        i32 IntValue   = 0;
        f32 FloatValue = 0.0f;
    };

    void SerializeTestData(FWorld& world, FComponentId id, ISerializer& serializer) {
        auto& component = world.ResolveComponent<FTestDataComponent>(id);
        serializer.Write(component.IntValue);
        serializer.Write(component.FloatValue);
    }

    void DeserializeTestData(FWorld& world, FComponentId id, IDeserializer& deserializer) {
        auto& component      = world.ResolveComponent<FTestDataComponent>(id);
        component.IntValue   = deserializer.Read<i32>();
        component.FloatValue = deserializer.Read<f32>();
    }

    void RegisterTestComponent() {
        auto entry        = BuildComponentTypeEntry<FTestDataComponent>();
        entry.Serialize   = &SerializeTestData;
        entry.Deserialize = &DeserializeTestData;
        GetComponentRegistry().Register(entry);
    }

    auto RequireTransformEqual(const Core::Math::LinAlg::FSpatialTransform& lhs,
        const Core::Math::LinAlg::FSpatialTransform&                        rhs) -> void {
        REQUIRE(lhs.Rotation.x == rhs.Rotation.x);
        REQUIRE(lhs.Rotation.y == rhs.Rotation.y);
        REQUIRE(lhs.Rotation.z == rhs.Rotation.z);
        REQUIRE(lhs.Rotation.w == rhs.Rotation.w);

        REQUIRE(lhs.Translation.mComponents[0] == rhs.Translation.mComponents[0]);
        REQUIRE(lhs.Translation.mComponents[1] == rhs.Translation.mComponents[1]);
        REQUIRE(lhs.Translation.mComponents[2] == rhs.Translation.mComponents[2]);

        REQUIRE(lhs.Scale.mComponents[0] == rhs.Scale.mComponents[0]);
        REQUIRE(lhs.Scale.mComponents[1] == rhs.Scale.mComponents[1]);
        REQUIRE(lhs.Scale.mComponents[2] == rhs.Scale.mComponents[2]);
    }
} // namespace

TEST_CASE("GameScene.World.Serialization.RoundTrip") {
    RegisterTestComponent();

    FWorld                                world(42);

    auto                                  root  = world.CreateGameObject(TEXT("Root"));
    auto                                  child = world.CreateGameObject(TEXT("Child"));

    Core::Math::LinAlg::FSpatialTransform rootTransform;
    rootTransform.Translation = Core::Math::FVector3f(1.0f, 2.0f, 3.0f);
    rootTransform.Scale       = Core::Math::FVector3f(1.5f, 1.0f, 0.5f);
    root.SetLocalTransform(rootTransform);

    Core::Math::LinAlg::FSpatialTransform childTransform;
    childTransform.Translation = Core::Math::FVector3f(-2.0f, 0.0f, 4.0f);
    childTransform.Scale       = Core::Math::FVector3f(0.75f, 0.75f, 0.75f);
    child.SetLocalTransform(childTransform);

    child.SetParent(root.GetId());
    child.SetActive(false);

    const auto rootComponentId  = world.CreateComponent<FTestDataComponent>(root.GetId());
    const auto childComponentId = world.CreateComponent<FTestDataComponent>(child.GetId());
    REQUIRE(rootComponentId.IsValid());
    REQUIRE(childComponentId.IsValid());

    auto& rootComp      = world.ResolveComponent<FTestDataComponent>(rootComponentId);
    rootComp.IntValue   = 7;
    rootComp.FloatValue = 1.25f;

    auto& childComp      = world.ResolveComponent<FTestDataComponent>(childComponentId);
    childComp.IntValue   = -4;
    childComp.FloatValue = 9.5f;
    childComp.SetEnabled(false);

    FBinarySerializer serializer;
    world.Serialize(serializer);

    FBinaryDeserializer deserializer;
    deserializer.SetBuffer(serializer.GetBuffer());
    auto loaded = FWorld::Deserialize(deserializer);
    REQUIRE(static_cast<bool>(loaded));

    REQUIRE(loaded->GetWorldId() == world.GetWorldId());
    REQUIRE(loaded->IsAlive(root.GetId()));
    REQUIRE(loaded->IsAlive(child.GetId()));

    const auto loadedRoot  = loaded->Object(root.GetId());
    const auto loadedChild = loaded->Object(child.GetId());

    REQUIRE(loadedChild.GetParent() == root.GetId());
    REQUIRE(loadedChild.IsActive() == false);

    RequireTransformEqual(loadedRoot.GetLocalTransform(), rootTransform);
    RequireTransformEqual(loadedChild.GetLocalTransform(), childTransform);

    const auto loadedRootComponentId  = loaded->GetComponent<FTestDataComponent>(root.GetId());
    const auto loadedChildComponentId = loaded->GetComponent<FTestDataComponent>(child.GetId());

    REQUIRE(loadedRootComponentId.IsValid());
    REQUIRE(loadedChildComponentId.IsValid());

    const auto& loadedRootComp =
        loaded->ResolveComponent<FTestDataComponent>(loadedRootComponentId);
    REQUIRE(loadedRootComp.IntValue == 7);
    REQUIRE(loadedRootComp.FloatValue == 1.25f);

    const auto& loadedChildComp =
        loaded->ResolveComponent<FTestDataComponent>(loadedChildComponentId);
    REQUIRE(loadedChildComp.IntValue == -4);
    REQUIRE(loadedChildComp.FloatValue == 9.5f);
    REQUIRE(loadedChildComp.IsEnabled() == false);
}
