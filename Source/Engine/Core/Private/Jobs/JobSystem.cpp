#include "../../Public/Jobs/JobSystem.h"
#include "../../Public/Threading/Mutex.h"
#include "../../Public/Threading/Atomic.h"
#include "../../Public/Container/Vector.h"
#include <algorithm>
#include <chrono>
#include <thread>
#include <iostream>
// Use engine public mutex header already included above (relative include present)
#include "../../Public/Threading/ConditionVariable.h"
#include "../../Public/Container/SmartPtr.h"
#include "../../Public/Container/HashMap.h"
#include "../../Public/Container/HashSet.h"

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

    // -------------------------------------------------------------------------
    // Basic job manager implementation (private runtime glue)
    // -------------------------------------------------------------------------

    struct JobState
    {
        AltinaEngine::Core::Threading::FMutex             mtx;
        AltinaEngine::Core::Threading::FConditionVariable cv;
        bool                                              completed = false;
    };

    static AltinaEngine::Core::Threading::TAtomic<u64> gNextJobId{ 1 };
    static AltinaEngine::Core::Threading::FMutex       gJobsMutex;
    using Container::MakeShared;
    using Container::THashMap;
    using Container::THashSet;
    using Container::TShared;
    static THashMap<u64, TShared<JobState>> gJobs;
    static FWorkerPool*                     gDefaultPool = nullptr;

    // Helper to ensure a default pool exists
    static FWorkerPool*                     EnsureDefaultPool()
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
    struct JobFence::Impl
    {
        AltinaEngine::Core::Threading::FMutex             mtx;
        AltinaEngine::Core::Threading::FConditionVariable cv;
        bool                                              signalled = false;
    };

    JobFence::JobFence() noexcept : mImpl(new Impl()) {}
    JobFence::~JobFence() noexcept { delete mImpl; }
    void JobFence::Wait() noexcept
    {
        if (!mImpl)
            return;
        AltinaEngine::Core::Threading::FScopedLock lk(mImpl->mtx);
        while (!mImpl->signalled)
        {
            mImpl->cv.Wait(mImpl->mtx);
        }
    }
    bool JobFence::WaitFor(u64 timeoutMs) noexcept
    {
        if (!mImpl)
            return true;
        AltinaEngine::Core::Threading::FScopedLock lk(mImpl->mtx);
        if (mImpl->signalled)
            return true;
        return mImpl->cv.Wait(mImpl->mtx, static_cast<unsigned long>(timeoutMs));
    }
    void JobFence::Signal() noexcept
    {
        if (!mImpl)
            return;
        {
            AltinaEngine::Core::Threading::FScopedLock lg(mImpl->mtx);
            mImpl->signalled = true;
        }
        mImpl->cv.NotifyAll();
    }
    bool JobFence::IsSignalled() const noexcept
    {
        if (!mImpl)
            return true;
        AltinaEngine::Core::Threading::FScopedLock lg(mImpl->mtx);
        return mImpl->signalled;
    }

    // DependencyNode implementation
    struct DependencyNode::Impl
    {
        TFunction<void()> Job;
        const char*       DebugLabel = nullptr;
        TVector<Impl*>    Prereqs;
    };

    void DependencyNode::AddPrerequisite(const DependencyNode& node) noexcept
    {
        if (!mImpl)
            mImpl = new Impl();
        if (node.mImpl)
            mImpl->Prereqs.PushBack(node.mImpl);
    }

    void DependencyNode::SetJob(TFunction<void()> job, const char* debugLabel) noexcept
    {
        if (!mImpl)
            mImpl = new Impl();
        mImpl->Job        = Move(job);
        mImpl->DebugLabel = debugLabel;
    }

    // Recursively emit nodes and collect their handles; prevent re-emitting nodes.
    static JobHandle EmitNodeRecursive(DependencyNode::Impl* node, THashMap<DependencyNode::Impl*, JobHandle>& emitted)
    {
        if (!node)
            return JobHandle{};
        auto it = emitted.find(node);
        if (it != emitted.end())
            return it->second;

        // Emit prerequisites first
        TVector<JobHandle> prereqHandles;
        std::cout << "EmitNodeRecursive: node=" << reinterpret_cast<void*>(node) << " prereqs=" << node->Prereqs.Size()
                  << "\n";
        for (usize i = 0; i < node->Prereqs.Size(); ++i)
        {
            auto h = EmitNodeRecursive(node->Prereqs[i], emitted);
            if (h.IsValid())
                prereqHandles.PushBack(h);
        }

        // Build wrapper that waits for prerequisites then runs job
        JobDescriptor desc;
        desc.DebugLabel = node->DebugLabel;
        std::cout << "EmitNodeRecursive: building callback, prereq count=" << prereqHandles.Size() << "\n";
        // Capture prereq handles by value. Copy the node's job into a heap
        // allocated TFunction so the lambda remains small and fits the
        // TFunction small-buffer.
        TShared<Container::TFunction<void()>> jobPtr;
        if (node->Job)
        {
            jobPtr = MakeShared<Container::TFunction<void()>>(node->Job);
        }

        desc.Callback = [jobPtr, prereqHandles]() mutable {
            for (usize i = 0; i < prereqHandles.Size(); ++i)
            {
                JobSystem::Wait(prereqHandles[i]);
            }
            if (jobPtr && static_cast<bool>(*jobPtr))
                (*jobPtr)();
        };

        std::cout << "EmitNodeRecursive: submitting callback\n";
        JobHandle h = JobSystem::Submit(desc);
        std::cout << "EmitNodeRecursive: submitted id=" << h.mId << "\n";
        emitted.emplace(node, h);
        return h;
    }

    JobHandle DependencyNode::Emit() const noexcept
    {
        if (!mImpl)
            return JobHandle{};
        THashMap<DependencyNode::Impl*, JobHandle> emitted;
        return EmitNodeRecursive(mImpl, emitted);
    }

    // JobSystem API
    JobHandle JobSystem::Submit(JobDescriptor desc) noexcept
    {
        const u64         id    = static_cast<u64>(gNextJobId.FetchAdd(1));
        TShared<JobState> state = MakeShared<JobState>();

        {
            AltinaEngine::Core::Threading::FScopedLock lg(gJobsMutex);
            gJobs.emplace(id, state);
        }

        // Ensure pool
        FWorkerPool*                          pool = EnsureDefaultPool();

        // Wrap the user's callback to mark completion.
        // Allocate the callback on the heap to keep the runtime lambda small
        // (TFunction uses a fixed small-buffer optimization without heap fallback).
        TShared<Container::TFunction<void()>> cbptr   = MakeShared<Container::TFunction<void()>>(Move(desc.Callback));
        Container::TFunction<void()>          wrapper = [cbptr, id, state]() mutable {
            try
            {
                if (cbptr && static_cast<bool>(*cbptr))
                    (*cbptr)();
            }
            catch (...)
            {
            }

            {
                AltinaEngine::Core::Threading::FScopedLock lg(state->mtx);
                state->completed = true;
            }
            state->cv.NotifyAll();
        };

        pool->Submit(Move(wrapper));
        return JobHandle(id);
    }

    JobHandle JobSystem::SubmitWithFence(JobDescriptor desc, JobFence& outFence) noexcept
    {
        const u64         id    = static_cast<u64>(gNextJobId.FetchAdd(1));
        TShared<JobState> state = MakeShared<JobState>();

        {
            AltinaEngine::Core::Threading::FScopedLock lg(gJobsMutex);
            gJobs.emplace(id, state);
        }

        FWorkerPool*                          pool = EnsureDefaultPool();

        TShared<Container::TFunction<void()>> cbptr   = MakeShared<Container::TFunction<void()>>(Move(desc.Callback));
        Container::TFunction<void()>          wrapper = [cbptr, state, &outFence]() mutable {
            try
            {
                if (cbptr && static_cast<bool>(*cbptr))
                    (*cbptr)();
            }
            catch (...)
            {
            }

            {
                AltinaEngine::Core::Threading::FScopedLock lg(state->mtx);
                state->completed = true;
            }
            state->cv.NotifyAll();
            outFence.Signal();
        };

        pool->Submit(Move(wrapper));
        return JobHandle(id);
    }

    void JobSystem::Wait(JobHandle h) noexcept
    {
        if (!h.IsValid())
            return;
        TShared<JobState> state;
        {
            AltinaEngine::Core::Threading::FScopedLock lg(gJobsMutex);
            auto                                       it = gJobs.find(h.mId);
            if (it == gJobs.end())
                return;
            state = it->second;
        }

        AltinaEngine::Core::Threading::FScopedLock lk(state->mtx);
        while (!state->completed)
        {
            state->cv.Wait(state->mtx);
        }
    }

    FWorkerPool* JobSystem::CreateWorkerPool(const FWorkerPoolConfig& cfg) noexcept
    {
        auto* p = new FWorkerPool(cfg);
        p->Start();
        return p;
    }

    void JobSystem::DestroyWorkerPool(FWorkerPool* pool) noexcept
    {
        if (!pool)
            return;
        pool->Stop();
        delete pool;
    }

} // namespace AltinaEngine::Core::Jobs
