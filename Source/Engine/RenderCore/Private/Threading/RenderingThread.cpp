#include "Threading/RenderingThread.h"

#include "Console/ConsoleVariable.h"
#include "Jobs/JobSystem.h"

#include <thread>

using AltinaEngine::Core::Container::FString;
using AltinaEngine::Core::Container::TFunction;
using AltinaEngine::Move;
namespace AltinaEngine::RenderCore {
    Core::Console::TConsoleVariable<i32> gRenderingThreadLagFrames(
        TEXT("gRenderingThreadLagFrames"),
        1);

    auto EnqueueRenderTask(FString TaskName, TFunction<void()> task) noexcept
        -> Core::Jobs::FJobHandle {
        Core::Jobs::FJobDescriptor desc{};
        desc.TaskName     = Move(TaskName);
        desc.AffinityMask = static_cast<u32>(Core::Jobs::ENamedThread::Rendering);
        desc.Callback     = Move(task);
        return Core::Jobs::FJobSystem::Submit(Move(desc));
    }

    FRenderingThread::FRenderingThread() noexcept = default;

    FRenderingThread::~FRenderingThread() { Stop(); }

    void FRenderingThread::Start() {
        if (mRunning.Exchange(1) != 0)
            return;

        mStopRequested.Store(0);
        mReady.Store(0);

        auto* thread = new std::thread([this]() -> void { ThreadMain(); });
        mThread      = thread;

        // Wait until the render thread registers with the job system.
        while (mReady.Load() == 0) {
            std::this_thread::yield();
        }
    }

    void FRenderingThread::Stop() {
        if (mRunning.Exchange(0) == 0)
            return;

        mStopRequested.Store(1);

        auto* thread = reinterpret_cast<std::thread*>(mThread);
        if (thread && thread->joinable())
            thread->join();
        delete thread;
        mThread = nullptr;
        mReady.Store(0);
    }

    auto FRenderingThread::IsRunning() const noexcept -> bool { return mRunning.Load() != 0; }

    void FRenderingThread::ThreadMain() {
        Core::Jobs::RegisterNamedThread(Core::Jobs::ENamedThread::Rendering, "RenderingThread");
        mReady.Store(1);

        while (mStopRequested.Load() == 0) {
            Core::Jobs::ProcessNamedThreadJobs(Core::Jobs::ENamedThread::Rendering);
            Core::Jobs::WaitForNamedThreadJobs(Core::Jobs::ENamedThread::Rendering, 16);
        }

        Core::Jobs::ProcessNamedThreadJobs(Core::Jobs::ENamedThread::Rendering);
        Core::Jobs::UnregisterNamedThread(Core::Jobs::ENamedThread::Rendering);
    }
} // namespace AltinaEngine::RenderCore










