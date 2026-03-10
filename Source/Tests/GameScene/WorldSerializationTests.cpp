#include "TestHarness.h"
#include "Base/AltinaBase.h"
#include "Engine/EngineReflection.h"
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

    auto ReadSerializedTransform(FBinaryDeserializer& deserializer)
        -> Core::Math::LinAlg::FSpatialTransform {
        Core::Math::LinAlg::FSpatialTransform transform{};
        transform.Rotation.x = deserializer.Read<f32>();
        transform.Rotation.y = deserializer.Read<f32>();
        transform.Rotation.z = deserializer.Read<f32>();
        transform.Rotation.w = deserializer.Read<f32>();

        transform.Translation.mComponents[0] = deserializer.Read<f32>();
        transform.Translation.mComponents[1] = deserializer.Read<f32>();
        transform.Translation.mComponents[2] = deserializer.Read<f32>();

        transform.Scale.mComponents[0] = deserializer.Read<f32>();
        transform.Scale.mComponents[1] = deserializer.Read<f32>();
        transform.Scale.mComponents[2] = deserializer.Read<f32>();
        return transform;
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

TEST_CASE("GameScene.World.SerializationV2.RawRecords") {
    AltinaEngine::Engine::RegisterEngineReflection();
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

    REQUIRE(deserializer.Read<u32>() == 2U);
    REQUIRE(deserializer.Read<u32>() == world.GetWorldId());
    REQUIRE(deserializer.Read<u32>() == 2U);

    // Root record.
    REQUIRE(deserializer.Read<u8>() == 0U);
    REQUIRE(deserializer.Read<u32>() == root.GetId().Index);
    REQUIRE(deserializer.Read<u32>() == root.GetId().Generation);
    REQUIRE(ReadSerializedString(deserializer).ToView() == TEXT("Root"));
    REQUIRE(deserializer.Read<bool>() == true);
    REQUIRE(deserializer.Read<bool>() == false);
    RequireTransformEqual(ReadSerializedTransform(deserializer), rootTransform);
    REQUIRE(deserializer.Read<u32>() == 1U);
    REQUIRE(deserializer.Read<FComponentTypeHash>() == GetComponentTypeHash<FTestDataComponent>());
    REQUIRE(deserializer.Read<bool>() == true);
    REQUIRE(deserializer.Read<i32>() == 7);
    REQUIRE(deserializer.Read<f32>() == 1.25f);

    // Child record.
    REQUIRE(deserializer.Read<u8>() == 0U);
    REQUIRE(deserializer.Read<u32>() == child.GetId().Index);
    REQUIRE(deserializer.Read<u32>() == child.GetId().Generation);
    REQUIRE(ReadSerializedString(deserializer).ToView() == TEXT("Child"));
    REQUIRE(deserializer.Read<bool>() == false);
    REQUIRE(deserializer.Read<bool>() == true);
    REQUIRE(deserializer.Read<u32>() == root.GetId().Index);
    REQUIRE(deserializer.Read<u32>() == root.GetId().Generation);
    RequireTransformEqual(ReadSerializedTransform(deserializer), childTransform);
    REQUIRE(deserializer.Read<u32>() == 1U);
    REQUIRE(deserializer.Read<FComponentTypeHash>() == GetComponentTypeHash<FTestDataComponent>());
    REQUIRE(deserializer.Read<bool>() == false);
    REQUIRE(deserializer.Read<i32>() == -4);
    REQUIRE(deserializer.Read<f32>() == 9.5f);

    REQUIRE(!deserializer.HasMoreData());
}
