#pragma once

#include "../Base/CoreAPI.h"
#include "../Types/Aliases.h"
#include "../Container/ThreadSafeQueue.h"
#include "../Threading/Event.h"
#include "../Container/Function.h"
#include <vector>
#include <thread>
#include <atomic>

namespace AltinaEngine::Core::Jobs {

struct FWorkerPoolConfig {
    usize MinThreads = 1;
    usize MaxThreads = 4;
    bool bAllowSteal = false; // reserved for future
};

class AE_CORE_API FWorkerPool {
public:
    explicit FWorkerPool(const FWorkerPoolConfig& InConfig = FWorkerPoolConfig()) noexcept;
    ~FWorkerPool() noexcept;

    void Start();
    void Stop();

    // Submit a job to the pool. Job is copied into the internal queue.
    void Submit(AltinaEngine::Core::Container::TFunction<void()> Job);

    bool IsRunning() const noexcept { return bRunning.load(); }

private:
    void WorkerMain();

    FWorkerPoolConfig Config;
    AltinaEngine::Core::Container::TThreadSafeQueue<AltinaEngine::Core::Container::TFunction<void()>> JobQueue;
    AltinaEngine::Core::Threading::FEvent WakeEvent{false, AltinaEngine::Core::Threading::EEventResetMode::Auto};
    std::vector<std::thread> Threads;
    std::atomic<bool> bRunning{false};
};

} // namespace
