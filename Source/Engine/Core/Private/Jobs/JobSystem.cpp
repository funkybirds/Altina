#include "../../Public/Jobs/JobSystem.h"
#include "../../Public/Threading/Mutex.h"
#include <utility>

namespace AltinaEngine::Core::Jobs {

FWorkerPool::FWorkerPool(const FWorkerPoolConfig& InConfig) noexcept
    : Config(InConfig)
{
}

FWorkerPool::~FWorkerPool() noexcept
{
    Stop();
}

void FWorkerPool::Start()
{
    if (bRunning.exchange(true)) return;

    const usize Count = Config.MinThreads > 0 ? Config.MinThreads : 1;
    Threads.reserve(Count);
    for (usize i = 0; i < Count; ++i) {
        Threads.emplace_back([this]() { WorkerMain(); });
    }
}

void FWorkerPool::Stop()
{
    if (!bRunning.exchange(false)) return;

    // Wake all workers so they exit promptly
    WakeEvent.Set();

    for (auto &t : Threads) {
        if (t.joinable()) t.join();
    }
    Threads.clear();
}

void FWorkerPool::Submit(AltinaEngine::Core::Container::TFunction<void()> Job)
{
    JobQueue.Push(std::move(Job));
    WakeEvent.Set();
}

void FWorkerPool::WorkerMain()
{
    while (bRunning.load() || !JobQueue.IsEmpty()) {
        // Drain jobs
        while (!JobQueue.IsEmpty()) {
            auto Task = JobQueue.Front();
            JobQueue.Pop();
            try {
                Task();
            } catch (...) {
                // Swallow exceptions for this prototype
            }
        }

        // Wait for new work
        if (bRunning.load()) {
            WakeEvent.Wait(1000); // wake periodically to re-check running flag
        }
    }
}

} // namespace
