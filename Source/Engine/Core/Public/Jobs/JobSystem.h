#pragma once

#include "../Base/CoreAPI.h"
#include "../Types/Aliases.h"
#include "../Container/ThreadSafeQueue.h"
#include "../Threading/Event.h"
#include "../Container/Function.h"

using AltinaEngine::i16;
using AltinaEngine::i32;
using AltinaEngine::i64;
using AltinaEngine::i8;
using AltinaEngine::isize;
using AltinaEngine::u16;
using AltinaEngine::u32;
using AltinaEngine::u64;
using AltinaEngine::u8;
using AltinaEngine::usize;

// Avoid leaking STL types in public headers; use engine containers and atomics.
#include "../Container/Vector.h"
#include "../Threading/Atomic.h"

// Bring common fixed-size aliases into the including translation unit for
// convenience and compatibility with existing tests that expect unqualified
// names like `u32` to be available.

namespace std {
    class thread;
}

namespace AltinaEngine::Core::Jobs {

    // Shorten commonly used engine types in this header to keep declarations concise.
    using Container::TFunction;
    using Container::TThreadSafeQueue;
    using Container::TVector;
    using Threading::FEvent;
    using Threading::FMutex;
    using Threading::TAtomic;

    struct FWorkerPoolConfig {
        usize mMinThreads = 1;
        usize mMaxThreads = 4;
        bool  mAllowSteal = false; // reserved for future
    };

    // Named thread identifiers (used as affinity mask bits). Consumers can set
    // `FJobDescriptor::AffinityMask` to route a job to a named thread instead
    // of the general worker pool. Values are bitflags so multiple targets can
    // be expressed (implementation will pick the first available).
    enum class ENamedThread : AltinaEngine::u32 {
        GameThread = 1u << 0,
        RHI        = 1u << 1,
        Rendering  = 1u << 2,
        Audio      = 1u << 3,
    };

    // Named thread registration helpers. These are intended for threads that manually
    // pump job queues (e.g. rendering thread). Do not add methods to FJobSystem for this.
    AE_CORE_API void RegisterNamedThread(ENamedThread thread, const char* name) noexcept;
    AE_CORE_API void UnregisterNamedThread(ENamedThread thread) noexcept;
    AE_CORE_API void ProcessNamedThreadJobs(ENamedThread thread) noexcept;
    AE_CORE_API auto WaitForNamedThreadJobs(ENamedThread thread,
        u64 timeoutMs = static_cast<u64>(Threading::kInfiniteWait)) noexcept -> bool;

    // Forward declare the pool type so the JobSystem API can reference it.
    class FWorkerPool;

    // A lightweight opaque handle to a submitted job. Handles are cheap value types
    // that can be used to wait for completion, query state, or form dependencies.
    struct FJobHandle {
        u64 mId = 0ULL;

        constexpr FJobHandle() noexcept = default;
        constexpr explicit FJobHandle(u64 id) noexcept : mId(id) {}

        [[nodiscard]] constexpr auto IsValid() const noexcept -> bool { return mId != 0ULL; }
    };

    // A simple fence object that can be signalled by producers and waited on by consumers.
    // Implementation will be provided in the private runtime; the public header exposes
    // the API surface required by consumers.
    class AE_CORE_API FJobFence {
    public:
        FJobFence() noexcept;
        ~FJobFence() noexcept;

        // Wait until this fence has been signalled. Blocks the caller until complete.
        void               Wait() noexcept;

        // Try waiting with timeout (milliseconds). Returns true if signalled.
        auto               WaitFor(u64 timeoutMs) noexcept -> bool;

        // Signal the fence (mark as complete).
        void               Signal() noexcept;

        // Query whether it's been signalled.
        [[nodiscard]] auto IsSignalled() const noexcept -> bool;

        // Non-copyable, non-movable
        FJobFence(const FJobFence&)                    = delete;
        auto operator=(const FJobFence&) -> FJobFence& = delete;

    private:
        struct Impl;
        Impl* mImpl = nullptr;
    };

    // NOTE: FDependencyNode has been removed. Dependencies are now expressed directly
    // on `FJobDescriptor` via the `Prerequisites` member (see below).

    // Descriptor used by submit APIs. Avoids leaking STL types: uses engine `TFunction` for
    // callbacks and raw pointers for optional payloads. Consumers must ensure payload lifetime if
    // used.
    struct FJobDescriptor {
        TFunction<void()> Callback;
        void*             Payload    = nullptr; // optional user data
        const char*       DebugLabel = nullptr;
        AltinaEngine::u32 AffinityMask =
            0; // mapping to named thread / pool ids (implementation-defined)
        int                 Priority = 0; // advisory priority
        // List of job handles this job depends on. The runtime will wait for
        // each prerequisite to complete before executing this job's callback.
        TVector<FJobHandle> Prerequisites;
    };

    // High-level job system API (static lifetime helpers). Implementations live in the private
    // module.
    namespace FJobSystem {
        // Submit a single job, returns a handle that can be waited upon.
        // Accepts the descriptor by value so the implementation can move callback payloads.
        AE_CORE_API auto Submit(FJobDescriptor desc) noexcept -> FJobHandle;

        // Submit a job and associate it with a fence that will be signalled on completion.
        AE_CORE_API auto SubmitWithFence(FJobDescriptor desc, FJobFence& outFence) noexcept
            -> FJobHandle;

        // Wait for a handle to complete. Returns immediately if handle invalid.
        AE_CORE_API void Wait(FJobHandle h) noexcept;

        // Register the current thread as the `GameThread` (main thread).
        // This sets up internal routing so jobs targeting `GameThread` will
        // be queued for the registered thread and must be drained via
        // `ProcessGameThreadJobs()` on that thread.
        AE_CORE_API void RegisterGameThread() noexcept;

        // Process queued work submitted to `GameThread`. Call from the
        // registered game thread to execute pending tasks targeted at it.
        AE_CORE_API void ProcessGameThreadJobs() noexcept;

        // Create/destroy worker pools (optional convenience)
        AE_CORE_API auto CreateWorkerPool(const FWorkerPoolConfig& cfg) noexcept -> FWorkerPool*;
        AE_CORE_API void DestroyWorkerPool(FWorkerPool* pool) noexcept;
    } // namespace FJobSystem

    class AE_CORE_API FWorkerPool {
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
        struct FJobEntry {
            TFunction<void()> mTask;
            int               mPriority    = 0;
            u64               mExecuteAtMs = 0; // milliseconds since epoch
        };

        TThreadSafeQueue<FJobEntry> mJobQueue;
        // Delayed jobs stored separately and moved to JobQueue when due
        TVector<FJobEntry>          mDelayedJobs;
        FMutex                      mDelayedJobsMutex;
        FEvent                      mWakeEvent{ false, Threading::EEventResetMode::Auto };
        TVector<void*> mThreads; // opaque thread pointers (implementation hides std::thread)
        TAtomic<i32>   mRunning{ static_cast<i32>(0) };
    };

} // namespace AltinaEngine::Core::Jobs
