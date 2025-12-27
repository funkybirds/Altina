#include "../../Public/Jobs/JobSystem.h"
#include "../../Public/Threading/Mutex.h"
#include "../../Public/Threading/Atomic.h"
#include "../../Public/Container/Vector.h"
#include <algorithm>
#include <chrono>
#include <thread>

namespace AltinaEngine::Core::Jobs
{

    FWorkerPool::FWorkerPool(const FWorkerPoolConfig& InConfig) noexcept : mConfig(InConfig) {}

    FWorkerPool::~FWorkerPool() noexcept { Stop(); }

    void FWorkerPool::Start()
    {
        if (mRunning.Exchange(1) != 0)
            return;

        const usize count = mConfig.mMinThreads > 0 ? mConfig.mMinThreads : 1;
        mThreads.Reserve(count);
        for (usize i = 0; i < count; ++i)
        {
            // allocate std::thread on heap and store opaque pointer in public TVector<void*>
            auto* t = new std::thread([this]() -> void { WorkerMain(); });
            mThreads.PushBack(reinterpret_cast<void*>(t));
        }
    }

    void FWorkerPool::Stop()
    {
        if (mRunning.Exchange(0) == 0)
            return;

        // Wake all workers so they exit promptly
        mWakeEvent.Set();

        for (usize i = 0; i < mThreads.Size(); ++i)
        {
            auto* tptr = reinterpret_cast<std::thread*>(mThreads[i]);
            if (tptr && tptr->joinable())
                tptr->join();
            delete tptr;
        }
        mThreads.Clear();
    }

    void FWorkerPool::Submit(TFunction<void()> Job)
    {
        FJobEntry e;
        e.mTask        = Move(Job);
        e.mPriority    = 0;
        e.mExecuteAtMs = static_cast<u64>(
            std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch())
                .count());
        mJobQueue.Push(Move(e));
        mWakeEvent.Set();
    }
    void FWorkerPool::SubmitDelayed(TFunction<void()> Job, u64 DelayMs)
    {
        FJobEntry e;
        e.mTask        = Move(Job);
        e.mPriority    = 0;
        e.mExecuteAtMs = static_cast<u64>(
            std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch())
                .count()
            + DelayMs);

        {
            AltinaEngine::Core::Threading::FScopedLock lock(mDelayedJobsMutex);
            mDelayedJobs.PushBack(Move(e));
        }
        mWakeEvent.Set();
    }
    void FWorkerPool::SubmitWithPriority(TFunction<void()> Job, int Priority)
    {
        FJobEntry e;
        e.mTask        = Move(Job);
        e.mPriority    = Priority;
        e.mExecuteAtMs = static_cast<u64>(
            std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch())
                .count());
        mJobQueue.Push(Move(e));
        mWakeEvent.Set();
    }

    void FWorkerPool::WorkerMain()
    {
        while (mRunning.Load() != 0 || !mJobQueue.IsEmpty())
        {
            // Move due delayed jobs into the main queue
            {
                AltinaEngine::Core::Threading::FScopedLock lock(mDelayedJobsMutex);
                auto nowMs = static_cast<u64>(std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::steady_clock::now().time_since_epoch())
                        .count());

                for (usize idx = 0; idx < mDelayedJobs.Size();)
                {
                    if (mDelayedJobs[idx].mExecuteAtMs <= nowMs)
                    {
                        mJobQueue.Push(Move(mDelayedJobs[idx]));
                        // remove current by swapping with last
                        if (idx + 1 < mDelayedJobs.Size())
                        {
                            mDelayedJobs[idx] = Move(mDelayedJobs.Back());
                        }
                        mDelayedJobs.PopBack();
                    }
                    else
                    {
                        ++idx;
                    }
                }
            }

            // Drain jobs into local vector to allow priority sorting
            TVector<FJobEntry> batch;
            while (!mJobQueue.IsEmpty())
            {
                auto item = mJobQueue.Front();
                mJobQueue.Pop();
                batch.PushBack(Move(item));
            }

            if (!batch.IsEmpty())
            {
                // Sort by priority descending
                std::sort(batch.begin(), batch.end(),
                    [](const FJobEntry& a, const FJobEntry& b) -> bool { return a.mPriority > b.mPriority; });

                auto now   = std::chrono::steady_clock::now();
                auto nowMs = static_cast<u64>(
                    std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count());
                // Execute ready tasks; re-queue delayed ones
                for (usize i = 0; i < batch.Size(); ++i)
                {
                    auto& j = batch[i];
                    if (j.mExecuteAtMs <= nowMs)
                    {
                        try
                        {
                            j.mTask();
                        }
                        catch (...)
                        {
                        }
                    }
                    else
                    {
                        AltinaEngine::Core::Threading::FScopedLock lock(mDelayedJobsMutex);
                        mDelayedJobs.PushBack(Move(j));
                    }
                }
            }

            // Wait for new work
            if (mRunning.Load() != 0)
            {
                mWakeEvent.Wait(1000); // wake periodically to re-check running flag
            }
        }
    }

} // namespace AltinaEngine::Core::Jobs
