#include "SpaceshipNativeScripts.h"

#include "Engine/GameScene/ComponentRegistry.h"
#include "Engine/GameScene/World.h"
#include "Reflection/Serializer.h"

#include "ShipCameraModesNative.h"
#include "ShipOrbitControllerNative.h"

namespace AltinaEngine::Demo::SpaceshipGame::NativeScripts {
    namespace {
        void SerializeNoop(GameScene::FWorld& /*world*/, GameScene::FComponentId /*id*/,
            Core::Reflection::ISerializer& /*serializer*/) {}

        void DeserializeNoop(GameScene::FWorld& /*world*/, GameScene::FComponentId /*id*/,
            Core::Reflection::IDeserializer& /*deserializer*/) {}
    } // namespace

    void RegisterSpaceshipNativeScripts() {
        auto& registry = GameScene::GetComponentRegistry();

        {
            const auto typeHash = GameScene::GetComponentTypeHash<FShipOrbitControllerNative>();
            if (!registry.Has(typeHash)) {
                auto entry      = GameScene::BuildComponentTypeEntry<FShipOrbitControllerNative>();
                entry.Serialize = &SerializeNoop;
                entry.Deserialize = &DeserializeNoop;
                registry.Register(entry);
            }
        }

        {
            const auto typeHash = GameScene::GetComponentTypeHash<FShipCameraModesNative>();
            if (!registry.Has(typeHash)) {
                auto entry        = GameScene::BuildComponentTypeEntry<FShipCameraModesNative>();
                entry.Serialize   = &SerializeNoop;
                entry.Deserialize = &DeserializeNoop;
                registry.Register(entry);
            }
        }
    }
} // namespace AltinaEngine::Demo::SpaceshipGame::NativeScripts
