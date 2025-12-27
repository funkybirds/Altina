#pragma once

#include "../Base/CoreAPI.h"
#include "../Types/Aliases.h"
#include "../Container/ThreadSafeQueue.h"
#include "../Threading/Event.h"
#include "../Container/Function.h"
// Avoid leaking STL types in public headers; use engine containers and atomics.
#include "../Container/Vector.h"
#include "../Threading/Atomic.h"

namespace std
{
    class thread;
}

namespace AltinaEngine::Core::Jobs
{

    // Shorten commonly used engine types in this header to keep declarations concise.
    using Container::TFunction;
    using Container::TThreadSafeQueue;
    using Container::TVector;
    using Threading::FEvent;
    using Threading::FMutex;
    using Threading::TAtomic;

    struct FWorkerPoolConfig
    {
        usize mMinThreads = 1;
        usize mMaxThreads = 4;
        bool  mAllowSteal = false; // reserved for future
    };

    class AE_CORE_API FWorkerPool
    {
    public:
        explicit FWorkerPool(const FWorkerPoolConfig& InConfig = FWorkerPoolConfig()) noexcept;
        ~FWorkerPool() noexcept;

        void Start();
        void Stop();

        // Submit a job to the pool. Job is copied into the internal queue.
        void Submit(TFunction<void()> Job);

        // Submit a job to be executed after the given delay (milliseconds)
        void SubmitDelayed(TFunction<void()> Job, u64 DelayMs);

        // Submit a job with a priority (higher value == higher priority). Priority is advisory.
        void SubmitWithPriority(TFunction<void()> Job, int Priority);

        auto IsRunning() const noexcept -> bool { return mRunning.Load() != 0; }

    private:
        void              WorkerMain();

        FWorkerPoolConfig mConfig;
        struct FJobEntry
        {
            TFunction<void()> mTask;
            int               mPriority    = 0;
            u64               mExecuteAtMs = 0; // milliseconds since epoch
        };

        TThreadSafeQueue<FJobEntry> mJobQueue;
        // Delayed jobs stored separately and moved to JobQueue when due
        TVector<FJobEntry>          mDelayedJobs;
        FMutex                      mDelayedJobsMutex;
        FEvent                      mWakeEvent{ false, Threading::EEventResetMode::Auto };
        TVector<void*>              mThreads; // opaque thread pointers (implementation hides std::thread)
        TAtomic<i32>                mRunning{ static_cast<i32>(0) };
    };

} // namespace AltinaEngine::Core::Jobs
