#include "TestHarness.h"

#include "Engine/EngineReflection.h"
#include "Engine/GameScene/CameraComponent.h"
#include "Engine/GameScene/ComponentRegistry.h"
#include "Engine/GameScene/Ids.h"
#include "Engine/GameScene/ScriptComponent.h"

using namespace AltinaEngine::GameScene;

TEST_CASE("GameScene.ScriptComponentRegistry.EngineAutoRegistration") {
    AltinaEngine::Engine::RegisterEngineReflection();

    auto& registry = GetComponentRegistry();
    REQUIRE(registry.Has(GetComponentTypeHash<FCameraComponent>()));
    REQUIRE(registry.Has(GetComponentTypeHash<FScriptComponent>()));
}
