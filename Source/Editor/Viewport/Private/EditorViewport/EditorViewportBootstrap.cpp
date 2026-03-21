#include "EditorViewport/EditorViewportBootstrap.h"

#include "Launch/RuntimeSession.h"
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
    }
} // namespace AltinaEngine::Editor::Viewport
