#include "TestHarness.h"

#include "../../Engine/Core/Public/Jobs/JobSystem.h"

#include <atomic>
#include <random>
#include <algorithm>
#include <utility>

using namespace AltinaEngine::Core;

TEST_CASE("FJobSystem Submit and Wait")
{
    using namespace AltinaEngine::Core::Jobs;

    bool           executed = false;

    FJobDescriptor desc;
    desc.Callback = [&]() { executed = true; };

    auto h = FJobSystem::Submit(desc);
    FJobSystem::Wait(h);

    REQUIRE(executed);
}

TEST_CASE("FJobSystem bottom-up merge sort using job dependencies")
{
    using namespace AltinaEngine::Core::Jobs;

    // Prepare data
    const int                          N = 1024;
    std::vector<int>                   a(N);
    std::mt19937                       rng(12345);
    std::uniform_int_distribution<int> dist(0, 1000000);
    for (int i = 0; i < N; ++i)
        a[i] = dist(rng);

    std::vector<int>    b(N);

    // prevHandles holds handles for runs of size `width` from previous pass
    TVector<FJobHandle> prevHandles;

    // Initial pass: runs of size 1 don't need jobs; we'll start merging at width=1
    std::vector<int>*   src = &a;
    std::vector<int>*   dst = &b;

    for (int width = 1; width < N; width *= 2)
    {
        TVector<FJobHandle> currHandles;
        currHandles.Reserve((N + (2 * width) - 1) / (2 * width));

        int segIndex = 0;
        for (int i = 0; i < N; i += 2 * width)
        {
            int            left  = i;
            int            mid   = std::min(i + width, N);
            int            right = std::min(i + 2 * width, N);

            FJobDescriptor desc;
            // Capture pointers and indices by value to avoid reference lifetime issues
            desc.Callback = [src, dst, left, mid, right]() {
                int li = left;
                int ri = mid;
                int wi = left;
                while (li < mid && ri < right)
                {
                    if ((*src)[li] <= (*src)[ri])
                        (*dst)[wi++] = (*src)[li++];
                    else
                        (*dst)[wi++] = (*src)[ri++];
                }
                while (li < mid)
                    (*dst)[wi++] = (*src)[li++];
                while (ri < right)
                    (*dst)[wi++] = (*src)[ri++];
            };

            // Determine prerequisites from previous level: each merged segment depends
            // on the two child runs' handles if they exist.
            if (width > 1)
            {
                // Each prev handle corresponds to a run of size `width` from previous round
                int leftChildIndex  = segIndex * 2;
                int rightChildIndex = leftChildIndex + 1;
                if (leftChildIndex < static_cast<int>(prevHandles.Size()))
                    desc.Prerequisites.PushBack(prevHandles[leftChildIndex]);
                if (rightChildIndex < static_cast<int>(prevHandles.Size()))
                    desc.Prerequisites.PushBack(prevHandles[rightChildIndex]);
            }

            auto h = FJobSystem::Submit(desc);
            currHandles.PushBack(h);
            ++segIndex;
        }

        // Wait for this pass to finish before swapping buffers and continuing
        // (could also create higher-level jobs with prerequisites instead of waiting here)
        for (int i = 0; i < static_cast<int>(currHandles.Size()); ++i)
            FJobSystem::Wait(currHandles[i]);

        prevHandles = std::move(currHandles);
        std::swap(src, dst);
    }

    // After passes, if sorted data is in `b`, copy back to `a`
    if (src != &a)
        a = b;

    REQUIRE(std::is_sorted(a.begin(), a.end()));
}

TEST_CASE("FJobSystem SubmitWithFence signals fence")
{
    using namespace AltinaEngine::Core::Jobs;

    FJobFence      fence;
    FJobDescriptor desc;
    desc.Callback = [&]() { /* quick work */ };

    auto h        = FJobSystem::SubmitWithFence(desc, fence);
    bool signaled = fence.WaitFor(2000);
    REQUIRE(signaled);
    REQUIRE(fence.IsSignalled());
    FJobSystem::Wait(h);
}

TEST_CASE("FJobDescriptor dependencies enforce ordering")
{
    using namespace AltinaEngine::Core::Jobs;

    std::atomic<int> counter{ 0 };

    FJobHandle       prevHandle;
    const int        kCount = 100;
    for (int i = 0; i < kCount; ++i)
    {
        FJobDescriptor desc;
        desc.Callback = [&counter]() {
            int v = counter.load();
            counter.store(v + 1);
        };

        if (prevHandle.IsValid())
        {
            desc.Prerequisites.PushBack(prevHandle);
        }

        prevHandle = FJobSystem::Submit(desc);
    }

    FJobSystem::Wait(prevHandle);
    REQUIRE_EQ(counter.load(), kCount);
}

TEST_CASE("FJobDescriptor complex dependency graph")
{
    using namespace AltinaEngine::Core::Jobs;

    const int        totalJobs = 100;
    const int        chains    = 4;
    const int        perChain  = totalJobs / chains;

    std::atomic<int> chainCounters[chains];
    for (int i = 0; i < chains; ++i)
        chainCounters[i].store(0);

    TVector<FJobHandle> chainEnds;
    chainEnds.Reserve(chains);

    // Build several independent ordered chains; each job in a chain increments
    // that chain's atomic counter using only load/store. Chains can run in
    // parallel but within a chain ordering is enforced by prerequisites.
    for (int c = 0; c < chains; ++c)
    {
        FJobHandle prev;
        for (int j = 0; j < perChain; ++j)
        {
            FJobDescriptor desc;
            desc.Callback = [&chainCounters, c]() {
                int v = chainCounters[c].load();
                chainCounters[c].store(v + 1);
            };
            if (prev.IsValid())
                desc.Prerequisites.PushBack(prev);
            prev = FJobSystem::Submit(desc);
        }
        chainEnds.PushBack(prev);
    }

    // Final join job depends on every chain end and verifies total count.
    bool           finalOk = false;
    FJobDescriptor finalDesc;
    finalDesc.Prerequisites = chainEnds;
    finalDesc.Callback      = [&]() {
        int sum = 0;
        for (int i = 0; i < chains; ++i)
            sum += chainCounters[i].load();
        finalOk = (sum == totalJobs);
    };

    auto finalH = FJobSystem::Submit(finalDesc);
    FJobSystem::Wait(finalH);
    REQUIRE(finalOk);
}
