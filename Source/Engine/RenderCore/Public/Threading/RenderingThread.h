#pragma once

#include "RenderCoreAPI.h"

#include "Threading/Atomic.h"
#include "Types/Aliases.h"

namespace AltinaEngine::Core::Console {
    class FConsoleVariable;
} // namespace AltinaEngine::Core::Console

namespace AltinaEngine::RenderCore {
    // Controls how many frames the game thread can lead the rendering thread.
    AE_RENDER_CORE_API extern Core::Console::FConsoleVariable* gRenderingThreadLagFrames;

    class AE_RENDER_CORE_API FRenderingThread final {
    public:
        FRenderingThread() noexcept;
        ~FRenderingThread();

        void Start();
        void Stop();
        [[nodiscard]] auto IsRunning() const noexcept -> bool;

    private:
        void ThreadMain();

        void*                       mThread        = nullptr;
        Core::Threading::TAtomic<i32> mRunning{ 0 };
        Core::Threading::TAtomic<i32> mStopRequested{ 0 };
        Core::Threading::TAtomic<i32> mReady{ 0 };
    };
} // namespace AltinaEngine::RenderCore
