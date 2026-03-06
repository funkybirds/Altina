#include "EditorViewport/EditorViewportBootstrap.h"

#include "Launch/RuntimeSession.h"
#include "Engine/GameScene/CameraComponent.h"
#include "Engine/GameScene/World.h"
#include "Engine/GameScene/WorldManager.h"

namespace AltinaEngine::Editor::Viewport {
    void FEditorViewportBootstrap::EnsureDefaultWorld(Launch::IRuntimeSession& session) const {
        auto services = session.GetServices();
        if (services.WorldManager == nullptr) {
            return;
        }

        if (services.WorldManager->GetActiveWorld() != nullptr) {
            return;
        }

        const auto worldHandle = services.WorldManager->CreateWorld();
        services.WorldManager->SetActiveWorld(worldHandle);
        auto* world = services.WorldManager->GetWorld(worldHandle);
        if (world == nullptr) {
            return;
        }

        auto cameraObject = world->CreateGameObject(TEXT("Editor.DefaultCamera"));
        auto cameraComp   = cameraObject.AddComponent<GameScene::FCameraComponent>();
        if (cameraComp.IsValid()) {
            auto& camera = cameraComp.Get();
            camera.SetNearPlane(0.1f);
            camera.SetFarPlane(1000.0f);
        }
    }
} // namespace AltinaEngine::Editor::Viewport
