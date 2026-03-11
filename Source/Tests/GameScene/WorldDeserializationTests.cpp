#include "TestHarness.h"

#include "Asset/AssetManager.h"
#include "Base/AltinaBase.h"
#include "Engine/EngineReflection.h"
#include "Engine/GameScene/CameraComponent.h"
#include "Engine/GameScene/ComponentRegistry.h"
#include "Engine/GameScene/World.h"
#include "Engine/GameSceneAsset/Prefab.h"
#include "Reflection/BinaryDeserializer.h"
#include "Reflection/BinarySerializer.h"
#include "Utility/Uuid.h"

using namespace AltinaEngine;
using namespace AltinaEngine::Core::Reflection;
using namespace AltinaEngine::GameScene;

namespace {
    constexpr const char* kDeserializePrefabLoaderType = "tests.prefab.deserialize.full";

    struct FDeserializeTestComponent final : public FComponent {
        i32 mIntValue   = 0;
        f32 mFloatValue = 0.0f;
    };

    void SerializeDeserializeTestData(FWorld& world, FComponentId id, ISerializer& serializer) {
        const auto& component = world.ResolveComponent<FDeserializeTestComponent>(id);
        serializer.Write(component.mIntValue);
        serializer.Write(component.mFloatValue);
    }

    void DeserializeDeserializeTestData(
        FWorld& world, FComponentId id, IDeserializer& deserializer) {
        auto& component       = world.ResolveComponent<FDeserializeTestComponent>(id);
        component.mIntValue   = deserializer.Read<i32>();
        component.mFloatValue = deserializer.Read<f32>();
    }

    void SerializeCameraTestData(FWorld& world, FComponentId id, ISerializer& serializer) {
        const auto& component = world.ResolveComponent<FCameraComponent>(id);
        serializer.Write(component.GetFovYRadians());
        serializer.Write(component.GetNearPlane());
        serializer.Write(component.GetFarPlane());
    }

    void DeserializeCameraTestData(FWorld& world, FComponentId id, IDeserializer& deserializer) {
        auto& component = world.ResolveComponent<FCameraComponent>(id);
        component.SetFovYRadians(deserializer.Read<f32>());
        component.SetNearPlane(deserializer.Read<f32>());
        component.SetFarPlane(deserializer.Read<f32>());
    }

    void RegisterDeserializeTestComponent() {
        auto entry        = BuildComponentTypeEntry<FDeserializeTestComponent>();
        entry.Serialize   = &SerializeDeserializeTestData;
        entry.Deserialize = &DeserializeDeserializeTestData;
        GetComponentRegistry().Register(entry);

        auto cameraEntry = BuildComponentTypeEntry<FCameraComponent>();
        if (const auto* existing =
                GetComponentRegistry().Find(GetComponentTypeHash<FCameraComponent>());
            existing != nullptr) {
            cameraEntry = *existing;
        }
        cameraEntry.Serialize   = &SerializeCameraTestData;
        cameraEntry.Deserialize = &DeserializeCameraTestData;
        GetComponentRegistry().Register(cameraEntry);
    }

    auto MakeUuid(u8 seed) -> FUuid {
        FUuid::FBytes bytes{};
        for (u32 i = 0U; i < FUuid::kByteCount; ++i) {
            bytes[i] = static_cast<u8>(seed + static_cast<u8>(i));
        }
        return FUuid(bytes);
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

    auto ContainsComponentId(const Core::Container::TVector<FComponentId>& list, FComponentId id)
        -> bool {
        for (const auto& entry : list) {
            if (entry == id) {
                return true;
            }
        }
        return false;
    }

    class FDeserializePrefabInstantiator final :
        public Engine::GameSceneAsset::FBasePrefabInstantiator {
    public:
        FDeserializePrefabInstantiator() : FBasePrefabInstantiator(kDeserializePrefabLoaderType) {}

        void Reset() {
            mCallCount       = 0;
            mLastHandle      = {};
            mLastRoot        = {};
            mLastSpawnedNode = {};
        }

        auto Instantiate(FWorld&       world, Asset::FAssetManager& /*manager*/,
            const Asset::FAssetHandle& assetHandle)
            -> Engine::GameSceneAsset::FPrefabInstantiateResult override {
            ++mCallCount;
            mLastHandle = assetHandle;

            // Force generation churn so loaded prefab root ID is remapped from archive root ID.
            auto temp = world.CreateGameObject(TEXT("DeserializePrefabTemp"));
            if (temp.IsValid()) {
                world.DestroyGameObject(temp);
            }

            auto root  = world.CreateGameObject(TEXT("DeserializePrefabRoot"));
            auto child = world.CreateGameObject(TEXT("DeserializePrefabChild"));
            child.SetParent(root.GetId());

            Engine::GameSceneAsset::FPrefabInstantiateResult result{};
            result.Root = root.GetId();
            result.SpawnedNodes.PushBack(root.GetId());
            result.SpawnedNodes.PushBack(child.GetId());

            mLastRoot        = result.Root;
            mLastSpawnedNode = child.GetId();
            return result;
        }

        i32                 mCallCount = 0;
        Asset::FAssetHandle mLastHandle{};
        FGameObjectId       mLastRoot{};
        FGameObjectId       mLastSpawnedNode{};
    };

    FDeserializePrefabInstantiator gDeserializePrefabInstantiator{};
} // namespace

TEST_CASE("GameScene.World.DeserializeV2.RawRoundTrip") {
    Engine::RegisterEngineReflection();
    RegisterDeserializeTestComponent();

    FWorld                                source(611);
    auto                                  root  = source.CreateGameObject(TEXT("RawRoot"));
    auto                                  child = source.CreateGameObject(TEXT("RawChild"));

    Core::Math::LinAlg::FSpatialTransform rootTransform{};
    rootTransform.Translation = Core::Math::FVector3f(1.0f, 2.0f, 3.0f);
    rootTransform.Scale       = Core::Math::FVector3f(2.0f, 1.0f, 0.5f);
    root.SetLocalTransform(rootTransform);

    Core::Math::LinAlg::FSpatialTransform childTransform{};
    childTransform.Translation = Core::Math::FVector3f(-4.0f, 0.5f, 8.0f);
    childTransform.Scale       = Core::Math::FVector3f(0.25f, 0.75f, 1.5f);
    child.SetLocalTransform(childTransform);
    child.SetParent(root.GetId());
    child.SetActive(false);

    const auto rootComponentId  = root.AddComponent<FDeserializeTestComponent>().GetId();
    const auto childComponentId = child.AddComponent<FDeserializeTestComponent>().GetId();
    REQUIRE(rootComponentId.IsValid());
    REQUIRE(childComponentId.IsValid());

    auto& rootComponent       = source.ResolveComponent<FDeserializeTestComponent>(rootComponentId);
    rootComponent.mIntValue   = 1024;
    rootComponent.mFloatValue = 1.75f;

    auto& childComponent     = source.ResolveComponent<FDeserializeTestComponent>(childComponentId);
    childComponent.mIntValue = -9;
    childComponent.mFloatValue = 23.5f;
    childComponent.SetEnabled(false);

    FBinarySerializer serializer;
    source.Serialize(serializer);

    FBinaryDeserializer deserializer;
    deserializer.SetBuffer(serializer.GetBuffer());

    Asset::FAssetManager assetManager{};
    auto                 loaded = FWorld::Deserialize(deserializer, assetManager);
    REQUIRE(loaded != nullptr);
    REQUIRE(loaded->GetWorldId() == source.GetWorldId());

    REQUIRE(loaded->IsAlive(root.GetId()));
    REQUIRE(loaded->IsAlive(child.GetId()));
    REQUIRE(loaded->Object(root.GetId()).IsActive());
    REQUIRE(!loaded->Object(child.GetId()).IsActive());
    REQUIRE(loaded->Object(child.GetId()).GetParent() == root.GetId());
    RequireTransformEqual(loaded->Object(root.GetId()).GetLocalTransform(), rootTransform);
    RequireTransformEqual(loaded->Object(child.GetId()).GetLocalTransform(), childTransform);

    const auto loadedRootComponentId =
        loaded->GetComponent<FDeserializeTestComponent>(root.GetId());
    const auto loadedChildComponentId =
        loaded->GetComponent<FDeserializeTestComponent>(child.GetId());
    REQUIRE(loadedRootComponentId.IsValid());
    REQUIRE(loadedChildComponentId.IsValid());

    const auto& loadedRootComponent =
        loaded->ResolveComponent<FDeserializeTestComponent>(loadedRootComponentId);
    const auto& loadedChildComponent =
        loaded->ResolveComponent<FDeserializeTestComponent>(loadedChildComponentId);

    REQUIRE(loadedRootComponent.mIntValue == 1024);
    REQUIRE(loadedRootComponent.mFloatValue == 1.75f);
    REQUIRE(loadedRootComponent.IsEnabled());

    REQUIRE(loadedChildComponent.mIntValue == -9);
    REQUIRE(loadedChildComponent.mFloatValue == 23.5f);
    REQUIRE(!loadedChildComponent.IsEnabled());
}

TEST_CASE("GameScene.World.DeserializeV2.PrefabInstantiateAndRemap") {
    Engine::RegisterEngineReflection();
    RegisterDeserializeTestComponent();

    gDeserializePrefabInstantiator.Reset();
    Engine::GameSceneAsset::GetPrefabInstantiatorRegistry().Register(
        gDeserializePrefabInstantiator);

    FWorld source(612);
    auto   rawParent  = source.CreateGameObject(TEXT("RawParent"));
    auto   prefabRoot = source.CreateGameObject(TEXT("SerializedPrefabRoot"));
    prefabRoot.SetParent(rawParent.GetId());
    prefabRoot.SetActive(false);

    Core::Math::LinAlg::FSpatialTransform prefabTransform{};
    prefabTransform.Translation = Core::Math::FVector3f(11.0f, -2.0f, 5.5f);
    prefabTransform.Scale       = Core::Math::FVector3f(0.8f, 1.2f, 0.9f);
    prefabRoot.SetLocalTransform(prefabTransform);

    Engine::GameSceneAsset::FPrefabDescriptor descriptor{};
    descriptor.LoaderType        = kDeserializePrefabLoaderType;
    descriptor.AssetHandle.mType = Asset::EAssetType::Model;
    descriptor.AssetHandle.mUuid = MakeUuid(7U);
    source.RegisterPrefabRoot(prefabRoot.GetId(), descriptor);

    FBinarySerializer serializer;
    source.Serialize(serializer);

    FBinaryDeserializer deserializer;
    deserializer.SetBuffer(serializer.GetBuffer());

    Asset::FAssetManager assetManager{};
    auto                 loaded = FWorld::Deserialize(deserializer, assetManager);
    REQUIRE(loaded != nullptr);
    REQUIRE(gDeserializePrefabInstantiator.mCallCount == 1);
    REQUIRE(gDeserializePrefabInstantiator.mLastHandle == descriptor.AssetHandle);

    const auto actualPrefabRoot = gDeserializePrefabInstantiator.mLastRoot;
    REQUIRE(actualPrefabRoot.IsValid());
    REQUIRE(actualPrefabRoot != prefabRoot.GetId());
    REQUIRE(loaded->IsAlive(actualPrefabRoot));
    REQUIRE(loaded->Object(actualPrefabRoot).GetParent() == rawParent.GetId());
    REQUIRE(!loaded->Object(actualPrefabRoot).IsActive());
    RequireTransformEqual(loaded->Object(actualPrefabRoot).GetLocalTransform(), prefabTransform);

    const auto* loadedDescriptor = loaded->TryGetPrefabDescriptor(actualPrefabRoot);
    REQUIRE(loadedDescriptor != nullptr);
    REQUIRE(loadedDescriptor->LoaderType.ToView() == descriptor.LoaderType.ToView());
    REQUIRE(loadedDescriptor->AssetHandle == descriptor.AssetHandle);

    REQUIRE(loaded->IsAlive(gDeserializePrefabInstantiator.mLastSpawnedNode));
    REQUIRE(loaded->Object(gDeserializePrefabInstantiator.mLastSpawnedNode).GetParent()
        == actualPrefabRoot);
}

TEST_CASE("GameScene.World.DeserializeV2.MixedRecordsAndActiveLists") {
    Engine::RegisterEngineReflection();
    RegisterDeserializeTestComponent();

    gDeserializePrefabInstantiator.Reset();
    Engine::GameSceneAsset::GetPrefabInstantiatorRegistry().Register(
        gDeserializePrefabInstantiator);

    FWorld source(613);

    auto   rawParent = source.CreateGameObject(TEXT("MixedRawParent"));
    auto   rawLeaf   = source.CreateGameObject(TEXT("MixedRawLeaf"));
    rawLeaf.SetParent(rawParent.GetId());
    rawLeaf.SetActive(false);

    auto prefabRoot = source.CreateGameObject(TEXT("MixedSerializedPrefabRoot"));
    prefabRoot.SetParent(rawParent.GetId());

    auto rawParentData = rawParent.AddComponent<FDeserializeTestComponent>();
    auto rawLeafData   = rawLeaf.AddComponent<FDeserializeTestComponent>();
    REQUIRE(rawParentData.IsValid());
    REQUIRE(rawLeafData.IsValid());
    rawParentData.Get().mIntValue   = 5;
    rawParentData.Get().mFloatValue = 6.5f;
    rawLeafData.Get().mIntValue     = -8;
    rawLeafData.Get().mFloatValue   = -3.25f;
    rawLeafData.Get().SetEnabled(false);

    const auto parentCameraId = rawParent.AddComponent<FCameraComponent>().GetId();
    const auto leafCameraId   = rawLeaf.AddComponent<FCameraComponent>().GetId();
    REQUIRE(parentCameraId.IsValid());
    REQUIRE(leafCameraId.IsValid());
    source.ResolveComponent<FCameraComponent>(leafCameraId).SetEnabled(false);

    Engine::GameSceneAsset::FPrefabDescriptor descriptor{};
    descriptor.LoaderType        = kDeserializePrefabLoaderType;
    descriptor.AssetHandle.mType = Asset::EAssetType::Model;
    descriptor.AssetHandle.mUuid = MakeUuid(21U);
    source.RegisterPrefabRoot(prefabRoot.GetId(), descriptor);

    FBinarySerializer serializer;
    source.Serialize(serializer);

    FBinaryDeserializer deserializer;
    deserializer.SetBuffer(serializer.GetBuffer());

    Asset::FAssetManager assetManager{};
    auto                 loaded = FWorld::Deserialize(deserializer, assetManager);
    REQUIRE(loaded != nullptr);
    REQUIRE(gDeserializePrefabInstantiator.mCallCount == 1);

    REQUIRE(loaded->IsAlive(rawParent.GetId()));
    REQUIRE(loaded->IsAlive(rawLeaf.GetId()));
    REQUIRE(!loaded->Object(rawLeaf.GetId()).IsActive());

    const auto loadedParentDataId =
        loaded->GetComponent<FDeserializeTestComponent>(rawParent.GetId());
    const auto loadedLeafDataId = loaded->GetComponent<FDeserializeTestComponent>(rawLeaf.GetId());
    REQUIRE(loadedParentDataId.IsValid());
    REQUIRE(loadedLeafDataId.IsValid());

    const auto& loadedParentData =
        loaded->ResolveComponent<FDeserializeTestComponent>(loadedParentDataId);
    const auto& loadedLeafData =
        loaded->ResolveComponent<FDeserializeTestComponent>(loadedLeafDataId);
    REQUIRE(loadedParentData.mIntValue == 5);
    REQUIRE(loadedParentData.mFloatValue == 6.5f);
    REQUIRE(loadedParentData.IsEnabled());
    REQUIRE(loadedLeafData.mIntValue == -8);
    REQUIRE(loadedLeafData.mFloatValue == -3.25f);
    REQUIRE(!loadedLeafData.IsEnabled());

    const auto loadedParentCameraId = loaded->GetComponent<FCameraComponent>(rawParent.GetId());
    const auto loadedLeafCameraId   = loaded->GetComponent<FCameraComponent>(rawLeaf.GetId());
    REQUIRE(loadedParentCameraId.IsValid());
    REQUIRE(loadedLeafCameraId.IsValid());
    REQUIRE(loaded->ResolveComponent<FCameraComponent>(loadedParentCameraId).IsEnabled());
    REQUIRE(!loaded->ResolveComponent<FCameraComponent>(loadedLeafCameraId).IsEnabled());

    const auto& activeCameras = loaded->GetActiveCameraComponents();
    REQUIRE(ContainsComponentId(activeCameras, loadedParentCameraId));
    REQUIRE(!ContainsComponentId(activeCameras, loadedLeafCameraId));

    const auto actualPrefabRoot = gDeserializePrefabInstantiator.mLastRoot;
    REQUIRE(actualPrefabRoot.IsValid());
    REQUIRE(loaded->IsAlive(actualPrefabRoot));
    REQUIRE(loaded->Object(actualPrefabRoot).GetParent() == rawParent.GetId());
}
