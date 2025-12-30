#include "../../Public/Jobs/JobSystem.h"
#include "../../Public/Threading/Mutex.h"
#include "../../Public/Threading/Atomic.h"
#include "../../Public/Container/Vector.h"
#include <algorithm>
#include <chrono>
#include <thread>
#include <iostream>
#include "../../Public/Instrumentation/Instrumentation.h"
// Use engine public mutex header already included above (relative include present)
#include "../../Public/Threading/ConditionVariable.h"
#include "../../Public/Container/SmartPtr.h"
#include "../../Public/Container/HashMap.h"
#include "../../Public/Container/HashSet.h"
#include "../../Public/Container/ThreadSafeQueue.h"

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
            auto* t = new std::thread([this]() -> void {
                AltinaEngine::Core::Instrumentation::SetCurrentThreadName("JobWorker");
                WorkerMain();
            });
            mThreads.PushBack(t);
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
        e.mTask     = Move(Job);
        e.mPriority = 0;
        e.mExecuteAtMs =
            std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch())
                .count()
            + DelayMs;

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
                Threading::FScopedLock lock(mDelayedJobsMutex);
                auto                   nowMs = static_cast<u64>(std::chrono::duration_cast<std::chrono::milliseconds>(
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
                        Threading::FScopedLock lock(mDelayedJobsMutex);
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

    // -------------------------------------------------------------------------
    // Basic job manager implementation (private runtime glue)
    // -------------------------------------------------------------------------

    struct JobState
    {
        FMutex                        mtx;
        Threading::FConditionVariable cv;
        bool                          completed = false;
    };

    static TAtomic<u64> gNextJobId{ 1 };
    static FMutex       gJobsMutex;
    using Container::MakeShared;
    using Container::THashMap;
    using Container::THashSet;
    using Container::TShared;
    static THashMap<u64, TShared<JobState>>                          gJobs;
    static FWorkerPool*                                              gDefaultPool = nullptr;
    // Queue for jobs submitted to the GameThread (main thread). Consumers
    // must call `FJobSystem::ProcessGameThreadJobs()` on the registered
    // game thread to execute these tasks.
    static Container::TThreadSafeQueue<Container::TFunction<void()>> gGameThreadQueue;
    static Threading::TAtomic<i32>                                   gGameThreadRegistered{ 0 };

    // Helper to ensure a default pool exists
    static auto                                                      EnsureDefaultPool() -> FWorkerPool*
    {
        AltinaEngine::Core::Threading::FScopedLock lg(gJobsMutex);
        if (!gDefaultPool)
        {
            gDefaultPool = new FWorkerPool(FWorkerPoolConfig{});
            gDefaultPool->Start();
        }
        return gDefaultPool;
    }

    // JobFence implementation
    struct FJobFence::Impl
    {
        FMutex                        mtx;
        Threading::FConditionVariable mCv;
        bool                          mSignalled = false;
    };

    FJobFence::FJobFence() noexcept : mImpl(new Impl()) {}
    FJobFence::~FJobFence() noexcept { delete mImpl; }
    void FJobFence::Wait() noexcept
    {
        if (!mImpl)
            return;
        Threading::FScopedLock lk(mImpl->mtx);
        while (!mImpl->mSignalled)
        {
            mImpl->mCv.Wait(mImpl->mtx);
        }
    }
    auto FJobFence::WaitFor(u64 timeoutMs) noexcept -> bool
    {
        if (!mImpl)
            return true;
        Threading::FScopedLock lk(mImpl->mtx);
        if (mImpl->mSignalled)
            return true;
        return mImpl->mCv.Wait(mImpl->mtx, static_cast<unsigned long>(timeoutMs));
    }
    void FJobFence::Signal() noexcept
    {
        if (!mImpl)
            return;
        {
            Threading::FScopedLock lg(mImpl->mtx);
            mImpl->mSignalled = true;
        }
        mImpl->mCv.NotifyAll();
    }
    auto FJobFence::IsSignalled() const noexcept -> bool
    {
        if (!mImpl)
            return true;
        Threading::FScopedLock lg(mImpl->mtx);
        return mImpl->mSignalled;
    }

    // Dependency nodes were removed; dependencies are declared on FJobDescriptor
    // and handled below in Submit/SubmitWithFence by waiting on `desc.Prerequisites`.

    // JobSystem API
    auto FJobSystem::Submit(FJobDescriptor desc) noexcept -> FJobHandle
    {
        const u64         id    = static_cast<u64>(gNextJobId.FetchAdd(1));
        TShared<JobState> state = MakeShared<JobState>();

        {
            Threading::FScopedLock lg(gJobsMutex);
            gJobs.emplace(id, state);
        }

        // If affinity targets a named thread, route accordingly. For now
        // we treat ENamedThread::GameThread specially and enqueue to the
        // game-thread queue; otherwise use the default worker pool.
        if ((desc.AffinityMask & static_cast<AltinaEngine::u32>(ENamedThread::GameThread)) != 0)
        {
            // Wrap the user's callback as before but push into game queue
            TShared<TFunction<void()>>   cbptr         = MakeShared<Container::TFunction<void()>>(Move(desc.Callback));
            TVector<FJobHandle>          prereqHandles = Move(desc.Prerequisites);

            Container::TFunction<void()> wrapper = [cbptr, prereqHandles, state]() mutable -> void {
                for (usize i = 0; i < prereqHandles.Size(); ++i)
                {
                    Wait(prereqHandles[i]);
                }
                try
                {
                    if (cbptr && static_cast<bool>(*cbptr))
                        (*cbptr)();
                }
                catch (...)
                {
                }

                {
                    Threading::FScopedLock lg(state->mtx);
                    state->completed = true;
                }
                state->cv.NotifyAll();
            };

            gGameThreadQueue.Push(Move(wrapper));
            return FJobHandle(id);
        }

        // Ensure pool
        FWorkerPool*                 pool = EnsureDefaultPool();

        // Wrap the user's callback to mark completion.
        // Allocate the callback on the heap to keep the runtime lambda small
        // (TFunction uses a fixed small-buffer optimization without heap fallback).
        TShared<TFunction<void()>>   cbptr         = MakeShared<Container::TFunction<void()>>(Move(desc.Callback));
        TVector<FJobHandle>          prereqHandles = Move(desc.Prerequisites);

        Container::TFunction<void()> wrapper = [cbptr, id, state, prereqHandles]() mutable -> void {
            // Wait for declared prerequisites
            for (usize i = 0; i < prereqHandles.Size(); ++i)
            {
                Wait(prereqHandles[i]);
            }

            try
            {
                if (cbptr && static_cast<bool>(*cbptr))
                    (*cbptr)();
            }
            catch (...)
            {
            }

            {
                Threading::FScopedLock lg(state->mtx);
                state->completed = true;
            }
            state->cv.NotifyAll();
        };

        pool->Submit(Move(wrapper));
        return FJobHandle(id);
    }

    auto FJobSystem::SubmitWithFence(FJobDescriptor desc, FJobFence& outFence) noexcept -> FJobHandle
    {
        const u64         id    = static_cast<u64>(gNextJobId.FetchAdd(1));
        TShared<JobState> state = MakeShared<JobState>();

        {
            Threading::FScopedLock lg(gJobsMutex);
            gJobs.emplace(id, state);
        }

        // Route to game-thread if requested
        if ((desc.AffinityMask & static_cast<AltinaEngine::u32>(ENamedThread::GameThread)) != 0)
        {
            TShared<TFunction<void()>> cbptr          = MakeShared<TFunction<void()>>(Move(desc.Callback));
            TVector<FJobHandle>        prereqHandles2 = Move(desc.Prerequisites);

            TFunction<void()>          wrapper = [cbptr, state, &outFence, prereqHandles2]() mutable -> void {
                for (usize i = 0; i < prereqHandles2.Size(); ++i)
                {
                    Wait(prereqHandles2[i]);
                }

                try
                {
                    if (cbptr && static_cast<bool>(*cbptr))
                        (*cbptr)();
                }
                catch (...)
                {
                }

                {
                    Threading::FScopedLock lg(state->mtx);
                    state->completed = true;
                }
                state->cv.NotifyAll();
                outFence.Signal();
            };

            gGameThreadQueue.Push(Move(wrapper));
            return FJobHandle(id);
        }

        FWorkerPool*               pool = EnsureDefaultPool();

        TShared<TFunction<void()>> cbptr          = MakeShared<TFunction<void()>>(Move(desc.Callback));
        TVector<FJobHandle>        prereqHandles2 = Move(desc.Prerequisites);

        TFunction<void()>          wrapper = [cbptr, state, &outFence, prereqHandles2]() mutable -> void {
            for (usize i = 0; i < prereqHandles2.Size(); ++i)
            {
                Wait(prereqHandles2[i]);
            }

            try
            {
                if (cbptr && static_cast<bool>(*cbptr))
                    (*cbptr)();
            }
            catch (...)
            {
            }

            {
                Threading::FScopedLock lg(state->mtx);
                state->completed = true;
            }
            state->cv.NotifyAll();
            outFence.Signal();
        };

        pool->Submit(Move(wrapper));
        return FJobHandle(id);
    }

    void FJobSystem::RegisterGameThread() noexcept
    {
        // Mark registered and set thread name for instrumentation
        gGameThreadRegistered.Store(1);
        AltinaEngine::Core::Instrumentation::SetCurrentThreadName("GameThread");
    }

    void FJobSystem::ProcessGameThreadJobs() noexcept
    {
        // Only the registered game thread should call this; we don't enforce that
        // but it's the intended usage. Drain and execute queued jobs.
        while (!gGameThreadQueue.IsEmpty())
        {
            auto job = gGameThreadQueue.Front();
            gGameThreadQueue.Pop();
            try
            {
                if (job && static_cast<bool>(job))
                    job();
            }
            catch (...)
            {
            }
        }
    }

    void FJobSystem::Wait(FJobHandle h) noexcept
    {
        if (!h.IsValid())
            return;
        TShared<JobState> state;
        {
            Threading::FScopedLock lg(gJobsMutex);
            auto                   it = gJobs.find(h.mId);
            if (it == gJobs.end())
                return;
            state = it->second;
        }

        Threading::FScopedLock lk(state->mtx);
        while (!state->completed)
        {
            state->cv.Wait(state->mtx);
        }
    }

    auto FJobSystem::CreateWorkerPool(const FWorkerPoolConfig& cfg) noexcept -> FWorkerPool*
    {
        auto* p = new FWorkerPool(cfg);
        p->Start();
        return p;
    }

    void FJobSystem::DestroyWorkerPool(FWorkerPool* pool) noexcept
    {
        if (!pool)
            return;
        pool->Stop();
        delete pool;
    }

} // namespace AltinaEngine::Core::Jobs
