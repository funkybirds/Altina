#include "TestHarness.h"

#include "Engine/GameScene/World.h"

namespace {
    using AltinaEngine::GameScene::FComponentId;
    using AltinaEngine::GameScene::FComponentTypeHash;
    using AltinaEngine::GameScene::FGameObjectId;
    using AltinaEngine::GameScene::FWorld;
    using AltinaEngine::GameScene::GetComponentTypeHash;
    using AltinaEngine::GameScene::FComponent;
    using AltinaEngine::u32;

    struct FTestComponent final : public FComponent {
        static int sCreateCount;
        static int sDestroyCount;

        int        mValue = 0;

        void OnCreate() override { ++sCreateCount; }
        void OnDestroy() override { ++sDestroyCount; }
    };

    int FTestComponent::sCreateCount  = 0;
    int FTestComponent::sDestroyCount = 0;

    void ResetCounters() {
        FTestComponent::sCreateCount  = 0;
        FTestComponent::sDestroyCount = 0;
    }
} // namespace

TEST_CASE("GameScene.World.GameObjectId.GenerationAndReuse") {
    FWorld world;

    const FGameObjectId first = world.CreateGameObject();
    REQUIRE(first.IsValid());

    const u32 firstIndex = first.Index;
    const u32 firstGen   = first.Generation;

    world.DestroyGameObject(first);
    REQUIRE(!world.IsAlive(first));

    const FGameObjectId second = world.CreateGameObject();
    REQUIRE(second.IsValid());
    REQUIRE_EQ(second.Index, firstIndex);
    REQUIRE(second.Generation != firstGen);
    REQUIRE_EQ(second.WorldId, world.GetWorldId());

    auto view = world.Object(second);
    REQUIRE(view.IsValid());
    REQUIRE(view.IsActive());
    view.SetActive(false);
    REQUIRE(!view.IsActive());
}

TEST_CASE("GameScene.World.ComponentId.GenerationAndReuse") {
    ResetCounters();

    FWorld world;
    auto   objectId = world.CreateGameObject();
    REQUIRE(objectId.IsValid());

    auto view = world.Object(objectId);
    auto comp = view.Add<FTestComponent>();
    REQUIRE(comp.IsValid());

    const FComponentId first = comp.GetId();
    REQUIRE(first.IsValid());
    REQUIRE(world.IsAlive(first));
    REQUIRE_EQ(FTestComponent::sCreateCount, 1);
    REQUIRE_EQ(comp.Get().GetOwner(), objectId);

    world.DestroyComponent(first);
    REQUIRE(!world.IsAlive(first));
    REQUIRE_EQ(FTestComponent::sDestroyCount, 1);

    const FComponentTypeHash typeHash = GetComponentTypeHash<FTestComponent>();
    const FComponentId       second   = world.Object(objectId).Add<FTestComponent>().GetId();
    REQUIRE(second.IsValid());
    REQUIRE_EQ(second.Type, typeHash);
    REQUIRE_EQ(second.Index, first.Index);
    REQUIRE(second.Generation != first.Generation);
    REQUIRE_EQ(FTestComponent::sCreateCount, 2);
}

TEST_CASE("GameScene.World.DestroyGameObjectDestroysComponents") {
    ResetCounters();

    FWorld world;
    auto   objectId = world.CreateGameObject();
    REQUIRE(objectId.IsValid());

    auto view = world.Object(objectId);
    auto comp = view.Add<FTestComponent>();
    REQUIRE(comp.IsValid());

    const FComponentId compId = comp.GetId();
    REQUIRE(world.IsAlive(compId));

    world.DestroyGameObject(objectId);
    REQUIRE(!world.IsAlive(objectId));
    REQUIRE(!world.IsAlive(compId));
    REQUIRE_EQ(FTestComponent::sDestroyCount, 1);
}




