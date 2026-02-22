#include "TestHarness.h"

#include "../../Runtime/Core/Public/Jobs/JobSystem.h"

using namespace AltinaEngine::Core::Jobs;

TEST_CASE("NamedThread: GameThread routing") {
    using namespace AltinaEngine::Core::Jobs;

    // Register current test thread as GameThread
    FJobSystem::RegisterGameThread();

    bool           executed = false;
    FJobDescriptor desc;
    desc.Callback     = [&]() { executed = true; };
    desc.AffinityMask = static_cast<u32>(ENamedThread::GameThread);

    auto h = FJobSystem::Submit(desc);

    // job should be queued to game thread and will run when we process the queue
    REQUIRE(!executed);
    FJobSystem::ProcessGameThreadJobs();
    REQUIRE(executed);

    FJobSystem::Wait(h);
}
