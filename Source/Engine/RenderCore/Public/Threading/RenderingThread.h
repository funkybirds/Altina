#pragma once

#include "RenderCoreAPI.h"

#include "Threading/Atomic.h"
#include "Types/Aliases.h"
#include "Console/ConsoleVariable.h"
#include "Jobs/JobSystem.h"

using AltinaEngine::Core::Container::FString;
using AltinaEngine::Core::Container::TFunction;
namespace AltinaEngine::RenderCore {
    // Controls how many frames the game thread can lead the rendering thread.
    AE_RENDER_CORE_API extern Core::Console::TConsoleVariable<i32> gRenderingThreadLagFrames;

    // Enqueue a render task from the game thread.
    AE_RENDER_CORE_API auto EnqueueRenderTask(FString TaskName, TFunction<void()> task) noexcept
        -> Core::Jobs::FJobHandle;

    class AE_RENDER_CORE_API FRenderingThread final {
    public:
        FRenderingThread() noexcept;
        ~FRenderingThread();

        void               Start();
        void               Stop();
        [[nodiscard]] auto IsRunning() const noexcept -> bool;

    private:
        void                          ThreadMain();

        void*                         mThread = nullptr;
        Core::Threading::TAtomic<i32> mRunning{ 0 };
        Core::Threading::TAtomic<i32> mStopRequested{ 0 };
        Core::Threading::TAtomic<i32> mReady{ 0 };
    };
} // namespace AltinaEngine::RenderCore
