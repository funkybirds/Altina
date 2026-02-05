#pragma once

#include "Base/LaunchAPI.h"
#include "CoreMinimal.h"
#include "Container/SmartPtr.h"
#include "Application/Application.h"
#include "Rhi/RhiContext.h"
#include "Rhi/RhiDevice.h"

namespace AltinaEngine::Launch {
    class AE_LAUNCH_API FEngineLoop final {
    public:
        FEngineLoop() = default;
        explicit FEngineLoop(const FStartupParameters& InStartupParameters);

        auto PreInit() -> bool;
        auto Init() -> bool;
        void Tick(float InDeltaTime);
        void Exit();

    private:
        Core::Container::TOwner<Application::FApplication> mApplication;
        Core::Container::TOwner<Rhi::FRhiContext>          mRhiContext;
        Core::Container::TShared<Rhi::FRhiDevice>          mRhiDevice;
        FStartupParameters mStartupParameters{};
        bool               mIsRunning = false;
    };
} // namespace AltinaEngine::Launch
