#pragma once

#include "../Base/CoreAPI.h"
#include "../Types/Aliases.h"
#include "../Container/ThreadSafeQueue.h"
#include "../Threading/Event.h"
#include "../Container/Function.h"
#include <chrono>
#include <vector>
#include <vector>
#include <thread>
#include <atomic>

namespace AltinaEngine::Core::Jobs
{

    struct FWorkerPoolConfig
    {
        usize MinThreads  = 1;
        usize MaxThreads  = 4;
        bool  bAllowSteal = false; // reserved for future
    };

    class AE_CORE_API FWorkerPool
    {
    public:
        explicit FWorkerPool(const FWorkerPoolConfig& InConfig = FWorkerPoolConfig()) noexcept;
        ~FWorkerPool() noexcept;

        void Start();
        void Stop();

        // Submit a job to the pool. Job is copied into the internal queue.
        void Submit(AltinaEngine::Core::Container::TFunction<void()> Job);

        // Submit a job to be executed after the given delay
        void SubmitDelayed(AltinaEngine::Core::Container::TFunction<void()> Job, std::chrono::milliseconds Delay);

        // Submit a job with a priority (higher value == higher priority). Priority is advisory.
        void SubmitWithPriority(AltinaEngine::Core::Container::TFunction<void()> Job, int Priority);

        bool IsRunning() const noexcept { return bRunning.load(); }

    private:
        void              WorkerMain();

        FWorkerPoolConfig Config;
        struct FJobEntry
        {
            AltinaEngine::Core::Container::TFunction<void()> Task;
            int                                              Priority  = 0;
            std::chrono::steady_clock::time_point            ExecuteAt = std::chrono::steady_clock::now();
        };

        AltinaEngine::Core::Container::TThreadSafeQueue<FJobEntry> JobQueue;
        // Delayed jobs stored separately and moved to JobQueue when due
        std::vector<FJobEntry>                                     DelayedJobs;
        AltinaEngine::Core::Threading::FMutex                      DelayedJobsMutex;
        AltinaEngine::Core::Threading::FEvent WakeEvent{ false, AltinaEngine::Core::Threading::EEventResetMode::Auto };
        std::vector<std::thread>              Threads;
        std::atomic<bool>                     bRunning{ false };
    };

} // namespace AltinaEngine::Core::Jobs
