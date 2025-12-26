#include "../../Public/Jobs/JobSystem.h"
#include "../../Public/Threading/Mutex.h"
#include "../../Public/Threading/Atomic.h"
#include "../../Public/Container/Vector.h"
#include <utility>
#include <algorithm>
#include <chrono>
#include <vector>
#include <thread>

namespace AltinaEngine::Core::Jobs
{

    FWorkerPool::FWorkerPool(const FWorkerPoolConfig& InConfig) noexcept : Config(InConfig) {}

    FWorkerPool::~FWorkerPool() noexcept { Stop(); }

    void FWorkerPool::Start()
    {
        if (bRunning.Exchange(1) != 0)
            return;

        const usize Count = Config.MinThreads > 0 ? Config.MinThreads : 1;
        Threads.Reserve(Count);
        for (usize i = 0; i < Count; ++i)
        {
            // allocate std::thread on heap and store opaque pointer in public TVector<void*>
            auto* t = new std::thread([this]() { WorkerMain(); });
            Threads.PushBack(reinterpret_cast<void*>(t));
        }
    }

    void FWorkerPool::Stop()
    {
        if (bRunning.Exchange(0) == 0)
            return;

        // Wake all workers so they exit promptly
        WakeEvent.Set();

        for (usize i = 0; i < Threads.Size(); ++i)
        {
            auto* tptr = reinterpret_cast<std::thread*>(Threads[i]);
            if (tptr && tptr->joinable())
                tptr->join();
            delete tptr;
        }
        Threads.Clear();
    }

    void FWorkerPool::Submit(TFunction<void()> Job)
    {
        FJobEntry e;
        e.Task       = Move(Job);
        e.Priority   = 0;
        e.ExecuteAtMs = static_cast<u64>(
            std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count());
        JobQueue.Push(Move(e));
        WakeEvent.Set();
    }
    void FWorkerPool::SubmitDelayed(TFunction<void()> Job, u64 DelayMs)
    {
        FJobEntry e;
        e.Task      = Move(Job);
        e.Priority  = 0;
        e.ExecuteAtMs = static_cast<u64>(
            std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count() + DelayMs);

        {
            AltinaEngine::Core::Threading::FScopedLock lock(DelayedJobsMutex);
            DelayedJobs.PushBack(Move(e));
        }
        WakeEvent.Set();
    }
    void FWorkerPool::SubmitWithPriority(TFunction<void()> Job, int Priority)
    {
        FJobEntry e;
        e.Task      = Move(Job);
        e.Priority  = Priority;
        e.ExecuteAtMs = static_cast<u64>(
            std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count());
        JobQueue.Push(Move(e));
        WakeEvent.Set();
    }

    void FWorkerPool::WorkerMain()
    {
        while (bRunning.Load() != 0 || !JobQueue.IsEmpty())
        {
            // Move due delayed jobs into the main queue
            {
                AltinaEngine::Core::Threading::FScopedLock lock(DelayedJobsMutex);
                auto nowMs = static_cast<u64>(
                    std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count());

                for (usize idx = 0; idx < DelayedJobs.Size();)
                {
                    if (DelayedJobs[idx].ExecuteAtMs <= nowMs)
                    {
                        JobQueue.Push(Move(DelayedJobs[idx]));
                        // remove current by swapping with last
                        if (idx + 1 < DelayedJobs.Size())
                        {
                            DelayedJobs[idx] = Move(DelayedJobs.Back());
                        }
                        DelayedJobs.PopBack();
                    }
                    else
                    {
                        ++idx;
                    }
                }
            }

            // Drain jobs into local vector to allow priority sorting
            TVector<FJobEntry> batch;
            while (!JobQueue.IsEmpty())
            {
                auto item = JobQueue.Front();
                JobQueue.Pop();
                batch.PushBack(Move(item));
            }

            if (!batch.IsEmpty())
            {
                // Sort by priority descending
                std::sort(batch.begin(), batch.end(),
                    [](const FJobEntry& a, const FJobEntry& b) { return a.Priority > b.Priority; });

                auto now = std::chrono::steady_clock::now();
                auto nowMs = static_cast<u64>(
                    std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count());
                // Execute ready tasks; re-queue delayed ones
                for (usize i = 0; i < batch.Size(); ++i)
                {
                    auto& j = batch[i];
                    if (j.ExecuteAtMs <= nowMs)
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
                        DelayedJobs.PushBack(Move(j));
                    }
                }
            }

            // Wait for new work
            if (bRunning.Load() != 0)
            {
                WakeEvent.Wait(1000); // wake periodically to re-check running flag
            }
        }
    }

} // namespace AltinaEngine::Core::Jobs
