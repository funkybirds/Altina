#include "../../Public/Jobs/JobSystem.h"
#include "../../Public/Threading/Mutex.h"
#include <utility>
#include <algorithm>
#include <chrono>
#include <vector>

namespace AltinaEngine::Core::Jobs
{

    FWorkerPool::FWorkerPool(const FWorkerPoolConfig& InConfig) noexcept : Config(InConfig) {}

    FWorkerPool::~FWorkerPool() noexcept { Stop(); }

    void FWorkerPool::Start()
    {
        if (bRunning.exchange(true))
            return;

        const usize Count = Config.MinThreads > 0 ? Config.MinThreads : 1;
        Threads.reserve(Count);
        for (usize i = 0; i < Count; ++i)
        {
            Threads.emplace_back([this]() { WorkerMain(); });
        }
    }

    void FWorkerPool::Stop()
    {
        if (!bRunning.exchange(false))
            return;

        // Wake all workers so they exit promptly
        WakeEvent.Set();

        for (auto& t : Threads)
        {
            if (t.joinable())
                t.join();
        }
        Threads.clear();
    }

    void FWorkerPool::Submit(AltinaEngine::Core::Container::TFunction<void()> Job)
    {
        FJobEntry e;
        e.Task      = std::move(Job);
        e.Priority  = 0;
        e.ExecuteAt = std::chrono::steady_clock::now();
        JobQueue.Push(std::move(e));
        WakeEvent.Set();
    }

    void FWorkerPool::SubmitDelayed(
        AltinaEngine::Core::Container::TFunction<void()> Job, std::chrono::milliseconds Delay)
    {
        FJobEntry e;
        e.Task      = std::move(Job);
        e.Priority  = 0;
        e.ExecuteAt = std::chrono::steady_clock::now() + Delay;

        {
            AltinaEngine::Core::Threading::FScopedLock lock(DelayedJobsMutex);
            DelayedJobs.push_back(std::move(e));
        }
        WakeEvent.Set();
    }

    void FWorkerPool::SubmitWithPriority(AltinaEngine::Core::Container::TFunction<void()> Job, int Priority)
    {
        FJobEntry e;
        e.Task      = std::move(Job);
        e.Priority  = Priority;
        e.ExecuteAt = std::chrono::steady_clock::now();
        JobQueue.Push(std::move(e));
        WakeEvent.Set();
    }

    void FWorkerPool::WorkerMain()
    {
        while (bRunning.load() || !JobQueue.IsEmpty())
        {
            // Move due delayed jobs into the main queue
            {
                AltinaEngine::Core::Threading::FScopedLock lock(DelayedJobsMutex);
                auto                                       now = std::chrono::steady_clock::now();
                for (auto it = DelayedJobs.begin(); it != DelayedJobs.end();)
                {
                    if (it->ExecuteAt <= now)
                    {
                        JobQueue.Push(std::move(*it));
                        it = DelayedJobs.erase(it);
                    }
                    else
                    {
                        ++it;
                    }
                }
            }

            // Drain jobs into local vector to allow priority sorting
            std::vector<FJobEntry> batch;
            while (!JobQueue.IsEmpty())
            {
                auto item = JobQueue.Front();
                JobQueue.Pop();
                batch.push_back(std::move(item));
            }

            if (!batch.empty())
            {
                // Sort by priority descending
                std::sort(batch.begin(), batch.end(),
                    [](const FJobEntry& a, const FJobEntry& b) { return a.Priority > b.Priority; });

                auto now = std::chrono::steady_clock::now();
                // Execute ready tasks; re-queue delayed ones
                for (auto& j : batch)
                {
                    if (j.ExecuteAt <= now)
                    {
                        try
                        {
                            j.Task();
                        }
                        catch (...)
                        {
                        }
                    }
                    else
                    {
                        AltinaEngine::Core::Threading::FScopedLock lock(DelayedJobsMutex);
                        DelayedJobs.push_back(std::move(j));
                    }
                }
            }

            // Wait for new work
            if (bRunning.load())
            {
                WakeEvent.Wait(1000); // wake periodically to re-check running flag
            }
        }
    }

} // namespace AltinaEngine::Core::Jobs
