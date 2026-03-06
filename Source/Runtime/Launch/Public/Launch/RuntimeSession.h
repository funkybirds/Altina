#pragma once

#include "Base/LaunchAPI.h"
#include "CoreMinimal.h"
#include "Container/SmartPtr.h"

namespace AltinaEngine::Application {
    class FPlatformWindow;
}

namespace AltinaEngine::Asset {
    class FAssetManager;
    class FAssetRegistry;
} // namespace AltinaEngine::Asset

namespace AltinaEngine::GameScene {
    class FWorldManager;
}

namespace AltinaEngine::Input {
    class FInputSystem;
}

namespace AltinaEngine::DebugGui {
    class IDebugGuiSystem;
}

namespace AltinaEngine::Launch {
    using AltinaEngine::Core::Container::TOwner;
    using AltinaEngine::Core::Container::TPolymorphicDeleter;

    struct FFrameContext {
        f32 DeltaSeconds = 1.0f / 60.0f;
        u64 FrameIndex   = 0ULL;
    };

    struct FSimulationTick {
        f32  DeltaSeconds = 1.0f / 60.0f;
        f32  TimeScale    = 1.0f;
        bool bSingleStep  = false;
    };

    struct FRenderTick {
        u32 RenderWidth  = 0U;
        u32 RenderHeight = 0U;
    };

    struct FRuntimeServices {
        Input::FInputSystem*          InputSystem    = nullptr;
        Application::FPlatformWindow* MainWindow     = nullptr;
        GameScene::FWorldManager*     WorldManager   = nullptr;
        Asset::FAssetRegistry*        AssetRegistry  = nullptr;
        Asset::FAssetManager*         AssetManager   = nullptr;
        DebugGui::IDebugGuiSystem*    DebugGuiSystem = nullptr;
    };

    struct FRuntimeServicesConst {
        const Input::FInputSystem*          InputSystem    = nullptr;
        const Application::FPlatformWindow* MainWindow     = nullptr;
        const GameScene::FWorldManager*     WorldManager   = nullptr;
        const Asset::FAssetRegistry*        AssetRegistry  = nullptr;
        const Asset::FAssetManager*         AssetManager   = nullptr;
        const DebugGui::IDebugGuiSystem*    DebugGuiSystem = nullptr;
    };

    class AE_LAUNCH_API IRuntimeSession {
    public:
        virtual ~IRuntimeSession() = default;

        virtual auto               PreInit() -> bool                                     = 0;
        virtual auto               Init() -> bool                                        = 0;
        virtual auto               BeginFrame(const FFrameContext& frameContext) -> bool = 0;
        virtual void               TickSimulation(const FSimulationTick& tick)           = 0;
        virtual void               RenderFrame(const FRenderTick& tick)                  = 0;
        virtual void               EndFrame()                                            = 0;
        virtual void               Shutdown()                                            = 0;
        [[nodiscard]] virtual auto GetServices() noexcept -> FRuntimeServices            = 0;
        [[nodiscard]] virtual auto GetServices() const noexcept -> FRuntimeServicesConst = 0;
        [[nodiscard]] virtual auto IsRunning() const noexcept -> bool                    = 0;
    };

    using FRuntimeSessionOwner = TOwner<IRuntimeSession, TPolymorphicDeleter<IRuntimeSession>>;

    AE_LAUNCH_API auto CreateDefaultRuntimeSession(const FStartupParameters& startupParameters)
        -> FRuntimeSessionOwner;
} // namespace AltinaEngine::Launch
