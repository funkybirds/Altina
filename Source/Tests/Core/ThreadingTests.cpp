#include "TestHarness.h"

#include "Threading/Mutex.h"
#include "Threading/ConditionVariable.h"
#include "Threading/Event.h"
#include "Threading/Atomic.h"

#include <thread>
#include <chrono>

using namespace AltinaEngine::Core::Threading;

TEST_CASE("FScopedLock releases mutex after scope") {
    FMutex M;
    {
        FScopedLock Lock(M);
        // Inside scope mutex is held; we don't assert TryLock behavior (platform-dependent)
    }
    // After scope the mutex must be unlocked
    bool Acquired = M.TryLock();
    REQUIRE(Acquired);
    if (Acquired)
        M.Unlock();
}

TEST_CASE("FConditionVariable notify wakes waiter") {
    FConditionVariable CV;
    FMutex             M;
    bool               Flag = false;

    std::thread        Worker([&]() {
        // Waiter thread
        M.Lock();
        bool Signaled = CV.Wait(M, 5000);
        // When signaled, Flag should be true
        REQUIRE(Signaled);
        REQUIRE(Flag);
        M.Unlock();
    });

    // Give worker time to start and wait
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    // Signal
    {
        FScopedLock Lock(M);
        Flag = true;
        CV.NotifyOne();
    }

    Worker.join();
}

TEST_CASE("FEvent signals waiter") {
    FEvent      E(false, EEventResetMode::Auto);

    bool        WorkerSaw = false;
    std::thread Worker([&]() {
        bool Signaled = E.Wait(5000);
        REQUIRE(Signaled);
        WorkerSaw = true;
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    E.Set();
    Worker.join();
    REQUIRE(WorkerSaw);
}

TEST_CASE("TAtomic concurrent increments") {
    using CounterT = int;
    TAtomic<CounterT>        Counter(static_cast<CounterT>(0));

    const int                Threads             = 4;
    const int                IncrementsPerThread = 10000;

    std::vector<std::thread> workers;
    for (int i = 0; i < Threads; ++i) {
        workers.emplace_back([&]() {
            for (int j = 0; j < IncrementsPerThread; ++j) {
                Counter.FetchAdd(1);
            }
        });
    }

    for (auto& t : workers)
        t.join();

    CounterT Final = Counter.Load();
    REQUIRE_EQ(Final, Threads * IncrementsPerThread);
}
