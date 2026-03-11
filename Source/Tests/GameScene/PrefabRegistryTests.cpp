#include "TestHarness.h"

#include "Asset/AssetManager.h"
#include "Engine/GameScene/World.h"
#include "Engine/GameSceneAsset/Prefab.h"
#include "Utility/Uuid.h"

namespace {
    using AltinaEngine::FUuid;
    using AltinaEngine::i32;
    using AltinaEngine::u32;
    using AltinaEngine::u8;
    namespace Asset          = AltinaEngine::Asset;
    namespace GameScene      = AltinaEngine::GameScene;
    namespace GameSceneAsset = AltinaEngine::Engine::GameSceneAsset;

    auto MakeUuid(u8 seed) -> FUuid {
        FUuid::FBytes bytes{};
        for (u32 i = 0U; i < FUuid::kByteCount; ++i) {
            bytes[i] = static_cast<u8>(seed + static_cast<u8>(i));
        }
        return FUuid(bytes);
    }

    class FCountingPrefabInstantiator final : public GameSceneAsset::FBasePrefabInstantiator {
    public:
        explicit FCountingPrefabInstantiator(
            AltinaEngine::Core::Container::FNativeStringView loaderType, i32 tag)
            : FBasePrefabInstantiator(loaderType), mTag(tag) {}

        auto Instantiate(GameScene::FWorld& world, Asset::FAssetManager& /*manager*/,
            const Asset::FAssetHandle&      assetHandle)
            -> GameSceneAsset::FPrefabInstantiateResult override {
            ++CallCount;
            LastTag    = mTag;
            LastHandle = assetHandle;

            GameSceneAsset::FPrefabInstantiateResult result{};
            auto object = world.CreateGameObject(TEXT("PrefabRegistry"));
            result.Root = object.GetId();
            result.SpawnedNodes.PushBack(result.Root);
            return result;
        }

        i32                 CallCount = 0;
        i32                 LastTag   = 0;
        Asset::FAssetHandle LastHandle{};

    private:
        i32 mTag = 0;
    };

    FCountingPrefabInstantiator gDispatchInstantiator("tests.prefab.registry.dispatch", 1);
    FCountingPrefabInstantiator gReplaceInstantiatorA("tests.prefab.registry.replace", 10);
    FCountingPrefabInstantiator gReplaceInstantiatorB("tests.prefab.registry.replace", 20);
} // namespace

TEST_CASE("GameScene.PrefabRegistry.Dispatch") {
    auto& registry = GameSceneAsset::GetPrefabInstantiatorRegistry();
    registry.Register(gDispatchInstantiator);

    Asset::FAssetManager manager{};
    GameScene::FWorld    world(501);
    Asset::FAssetHandle  handle{};
    handle.mType = Asset::EAssetType::Model;
    handle.mUuid = MakeUuid(3U);

    const auto* found = registry.Find("tests.prefab.registry.dispatch");
    REQUIRE(found == &gDispatchInstantiator);

    const auto result =
        registry.Instantiate("tests.prefab.registry.dispatch", world, manager, handle);
    REQUIRE(result.Root.IsValid());
    REQUIRE(result.SpawnedNodes.Size() == 1U);
    REQUIRE(gDispatchInstantiator.CallCount >= 1);
    REQUIRE(gDispatchInstantiator.LastTag == 1);
    REQUIRE(gDispatchInstantiator.LastHandle == handle);
}

TEST_CASE("GameScene.PrefabRegistry.ReplaceExisting") {
    auto& registry = GameSceneAsset::GetPrefabInstantiatorRegistry();
    registry.Register(gReplaceInstantiatorA);
    registry.Register(gReplaceInstantiatorB);

    Asset::FAssetManager manager{};
    GameScene::FWorld    world(502);
    Asset::FAssetHandle  handle{};
    handle.mType = Asset::EAssetType::Model;
    handle.mUuid = MakeUuid(8U);

    const auto result =
        registry.Instantiate("tests.prefab.registry.replace", world, manager, handle);
    REQUIRE(result.Root.IsValid());
    REQUIRE(gReplaceInstantiatorB.CallCount >= 1);
    REQUIRE(gReplaceInstantiatorB.LastTag == 20);
}
