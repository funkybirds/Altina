#include "TestHarness.h"

#include "Jobs/JobSystem.h"

#include <atomic>

using namespace AltinaEngine::Core;

TEST_CASE("JobSystem Submit and Wait")
{
    using namespace AltinaEngine::Core::Jobs;

    bool          executed = false;

    JobDescriptor desc;
    desc.Callback = [&]() { executed = true; };

    auto h = JobSystem::Submit(desc);
    JobSystem::Wait(h);

    REQUIRE(executed);
}

TEST_CASE("JobSystem SubmitWithFence signals fence")
{
    using namespace AltinaEngine::Core::Jobs;

    JobFence      fence;
    JobDescriptor desc;
    desc.Callback = [&]() { /* quick work */ };

    auto h        = JobSystem::SubmitWithFence(desc, fence);
    bool signaled = fence.WaitFor(2000);
    REQUIRE(signaled);
    REQUIRE(fence.IsSignalled());
    JobSystem::Wait(h);
}

TEST_CASE("DependencyNode enforces ordering")
{
    using namespace AltinaEngine::Core::Jobs;

    std::atomic<int> aVal{ 0 };
    int              bRead = 0;

    DependencyNode   A;
    DependencyNode   B;

    A.SetJob([&]() { aVal.store(42); });
    B.AddPrerequisite(A);
    B.SetJob([&]() { bRead = aVal.load(); });

    std::cout << "JobSystem test: before Emit\n";
    auto h = B.Emit();
    std::cout << "JobSystem test: after Emit\n";
    JobSystem::Wait(h);
    std::cout << "JobSystem test: after Wait\n";

    REQUIRE_EQ(aVal.load(), 42);
    REQUIRE_EQ(bRead, 42);
}
