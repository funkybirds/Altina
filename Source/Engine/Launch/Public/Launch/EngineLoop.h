#pragma once

#include "Base/LaunchAPI.h"
#include "CoreMinimal.h"
#include "Container/SmartPtr.h"
#include "Application/Application.h"
#include "Rhi/RhiContext.h"
#include "Rhi/RhiDevice.h"
#include "Rhi/RhiRefs.h"
#include "Rhi/RhiViewport.h"

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
        Core::Container::TOwner<Application::FApplication,
            Core::Container::TPolymorphicDeleter<Application::FApplication>>
            mApplication;
        Core::Container::TOwner<Rhi::FRhiContext,
            Core::Container::TPolymorphicDeleter<Rhi::FRhiContext>>
            mRhiContext;
        Core::Container::TShared<Rhi::FRhiDevice>          mRhiDevice;
        Rhi::FRhiViewportRef                               mMainViewport;
        u32                                               mViewportWidth = 0U;
        u32                                               mViewportHeight = 0U;
        u64                                               mFrameIndex = 0ULL;
        FStartupParameters mStartupParameters{};
        bool               mIsRunning = false;
    };
} // namespace AltinaEngine::Launch
