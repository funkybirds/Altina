#include "TestHarness.h"

#include "../../Engine/Core/Public/Jobs/JobSystem.h"
#include "../../Engine/Core/Public/Instrumentation/Instrumentation.h"

#include <mutex>
#include <string>
#include <thread>

using namespace AltinaEngine::Core;
using namespace AltinaEngine::Core::Jobs;
using namespace AltinaEngine::Core::Instrumentation;

TEST_CASE("FWorkerPool: worker thread sets instrumentation name")
{
    // Create a small pool with a single worker and verify that the worker
    // thread sets its instrumentation name to "JobWorker" when running.
    FWorkerPoolConfig cfg;
    cfg.mMinThreads = 1;

    FWorkerPool* pool = FJobSystem::CreateWorkerPool(cfg);
    REQUIRE(pool != nullptr);

    std::mutex  mtx;
    std::string recordedName;

    // Submit a job directly to the pool so it executes on the worker thread.
    pool->Submit([&]() {
        const char*                 n = GetCurrentThreadName();
        std::lock_guard<std::mutex> lk(mtx);
        if (n)
            recordedName = n;
    });

    // Wait for the worker to run and record the name (timeout to avoid hangs).
    const int maxWaitMs = 2000;
    int       waited    = 0;
    while (true)
    {
        {
            std::lock_guard<std::mutex> lk(mtx);
            if (!recordedName.empty())
                break;
        }
        if (waited >= maxWaitMs)
            break;
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
        waited += 1;
    }

    // Tear down pool
    FJobSystem::DestroyWorkerPool(pool);

    REQUIRE(!recordedName.empty());
    REQUIRE(recordedName == "JobWorker");
}
