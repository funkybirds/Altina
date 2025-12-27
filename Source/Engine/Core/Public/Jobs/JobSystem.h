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

    // Forward declare the pool type so the JobSystem API can reference it.
    class FWorkerPool;

    // A lightweight opaque handle to a submitted job. Handles are cheap value types
    // that can be used to wait for completion, query state, or form dependencies.
    struct JobHandle
    {
        u64 mId = 0ULL;

        constexpr JobHandle() noexcept = default;
        constexpr explicit JobHandle(u64 id) noexcept : mId(id) {}

        [[nodiscard]] constexpr bool IsValid() const noexcept { return mId != 0ULL; }
    };

    // A simple fence object that can be signalled by producers and waited on by consumers.
    // Implementation will be provided in the private runtime; the public header exposes
    // the API surface required by consumers.
    class AE_CORE_API JobFence
    {
    public:
        JobFence() noexcept;
        ~JobFence() noexcept;

        // Wait until this fence has been signalled. Blocks the caller until complete.
        void Wait() noexcept;

        // Try waiting with timeout (milliseconds). Returns true if signalled.
        bool WaitFor(u64 timeoutMs) noexcept;

        // Signal the fence (mark as complete).
        void Signal() noexcept;

        // Query whether it's been signalled.
        bool IsSignalled() const noexcept;

        // Non-copyable, non-movable
        JobFence(const JobFence&)            = delete;
        JobFence& operator=(const JobFence&) = delete;

    private:
        struct Impl;
        Impl* mImpl = nullptr;
    };

    // Dependency node represents a graph node that can depend on other nodes and produce
    // a JobHandle once emitted. This is an ergonomic helper for building job graphs.
    class AE_CORE_API DependencyNode
    {
    public:
        struct Impl;
        DependencyNode() noexcept  = default;
        ~DependencyNode() noexcept = default;

        // Add a prerequisite node which must complete before this node executes.
        void      AddPrerequisite(const DependencyNode& node) noexcept;

        // Attach a job descriptor to this node (single job). Replacing is allowed.
        void      SetJob(TFunction<void()> job, const char* debugLabel = "") noexcept;

        // Emit this node to the runtime and receive a JobHandle for waiting.
        JobHandle Emit() const noexcept;

        // Non-copyable
        DependencyNode(const DependencyNode&)                    = delete;
        auto operator=(const DependencyNode&) -> DependencyNode& = delete;

    private:
        Impl* mImpl = nullptr;
    };

    // Descriptor used by submit APIs. Avoids leaking STL types: uses engine `TFunction` for callbacks
    // and raw pointers for optional payloads. Consumers must ensure payload lifetime if used.
    struct JobDescriptor
    {
        TFunction<void()> Callback;
        void*             Payload      = nullptr; // optional user data
        const char*       DebugLabel   = nullptr;
        u32               AffinityMask = 0; // mapping to named thread / pool ids (implementation-defined)
        int               Priority     = 0; // advisory priority
    };

    // High-level job system API (static lifetime helpers). Implementations live in the private module.
    namespace JobSystem
    {
        // Submit a single job, returns a handle that can be waited upon.
        // Accepts the descriptor by value so the implementation can move callback payloads.
        AE_CORE_API JobHandle    Submit(JobDescriptor desc) noexcept;

        // Submit a job and associate it with a fence that will be signalled on completion.
        AE_CORE_API JobHandle    SubmitWithFence(JobDescriptor desc, JobFence& outFence) noexcept;

        // Wait for a handle to complete. Returns immediately if handle invalid.
        AE_CORE_API void         Wait(JobHandle h) noexcept;

        // Create/destroy worker pools (optional convenience)
        AE_CORE_API FWorkerPool* CreateWorkerPool(const FWorkerPoolConfig& cfg) noexcept;
        AE_CORE_API void         DestroyWorkerPool(FWorkerPool* pool) noexcept;
    } // namespace JobSystem

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
