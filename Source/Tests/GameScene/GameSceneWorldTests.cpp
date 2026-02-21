#include "TestHarness.h"

#include "Engine/GameScene/World.h"

namespace {
    using AltinaEngine::u32;
    using AltinaEngine::GameScene::FComponent;
    using AltinaEngine::GameScene::FComponentId;
    using AltinaEngine::GameScene::FComponentTypeHash;
    using AltinaEngine::GameScene::FGameObjectId;
    using AltinaEngine::GameScene::FWorld;
    using AltinaEngine::GameScene::GetComponentTypeHash;

    struct FTestComponent final : public FComponent {
        static int sCreateCount;
        static int sDestroyCount;

        int        mValue = 0;

        void       OnCreate() override { ++sCreateCount; }
        void       OnDestroy() override { ++sDestroyCount; }
    };

    int  FTestComponent::sCreateCount  = 0;
    int  FTestComponent::sDestroyCount = 0;

    void ResetCounters() {
        FTestComponent::sCreateCount  = 0;
        FTestComponent::sDestroyCount = 0;
    }
} // namespace

TEST_CASE("GameScene.World.GameObjectId.GenerationAndReuse") {
    FWorld              world;

    const auto firstView = world.CreateGameObject();
    REQUIRE(firstView.IsValid());
    const FGameObjectId first = firstView.GetId();

    const u32 firstIndex = first.Index;
    const u32 firstGen   = first.Generation;

    world.DestroyGameObject(firstView);
    REQUIRE(!world.IsAlive(first));

    const auto          secondView = world.CreateGameObject();
    REQUIRE(secondView.IsValid());
    const FGameObjectId second = secondView.GetId();
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
    auto objectView = world.CreateGameObject();
    REQUIRE(objectView.IsValid());

    const auto objectId = objectView.GetId();
    auto       comp     = objectView.AddComponent<FTestComponent>();
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
    const FComponentId       second   =
        world.Object(objectId).AddComponent<FTestComponent>().GetId();
    REQUIRE(second.IsValid());
    REQUIRE_EQ(second.Type, typeHash);
    REQUIRE_EQ(second.Index, first.Index);
    REQUIRE(second.Generation != first.Generation);
    REQUIRE_EQ(FTestComponent::sCreateCount, 2);
}

TEST_CASE("GameScene.World.DestroyGameObjectDestroysComponents") {
    ResetCounters();

    FWorld world;
    auto objectView = world.CreateGameObject();
    REQUIRE(objectView.IsValid());

    const auto objectId = objectView.GetId();
    auto       comp     = objectView.AddComponent<FTestComponent>();
    REQUIRE(comp.IsValid());

    const FComponentId compId = comp.GetId();
    REQUIRE(world.IsAlive(compId));

    world.DestroyGameObject(objectView);
    REQUIRE(!world.IsAlive(objectId));
    REQUIRE(!world.IsAlive(compId));
    REQUIRE_EQ(FTestComponent::sDestroyCount, 1);
}
