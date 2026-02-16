#include "TestHarness.h"

#include "Container/Variant.h"

using namespace AltinaEngine;
using namespace AltinaEngine::Core::Container;

namespace {
    struct FTracker {
        static int ctorCount;
        static int dtorCount;
        static int copyCount;
        static int moveCount;

        int value = 0;

        FTracker() : value(0) { ++ctorCount; }
        explicit FTracker(int v) : value(v) { ++ctorCount; }
        FTracker(const FTracker& other) : value(other.value) { ++copyCount; }
        FTracker(FTracker&& other) noexcept : value(other.value) {
            ++moveCount;
            other.value = -1;
        }
        ~FTracker() { ++dtorCount; }
    };

    int FTracker::ctorCount = 0;
    int FTracker::dtorCount = 0;
    int FTracker::copyCount = 0;
    int FTracker::moveCount = 0;

    void ResetTrackerCounts() {
        FTracker::ctorCount = 0;
        FTracker::dtorCount = 0;
        FTracker::copyCount = 0;
        FTracker::moveCount = 0;
    }
} // namespace

TEST_CASE("Variant basic ops") {
    using TVar = TVariant<int, float>;
    TVar v;
    REQUIRE(!v.HasValue());
    REQUIRE_EQ(v.Index(), TVar::kInvalidIndex);

    v.Emplace<int>(42);
    REQUIRE(v.HasValue());
    REQUIRE(v.Is<int>());
    REQUIRE_EQ(v.Index(), 0U);
    REQUIRE_EQ(v.Get<int>(), 42);
    REQUIRE(v.TryGet<float>() == nullptr);

    v = 2.5f;
    REQUIRE(v.Is<float>());
    REQUIRE_EQ(v.Index(), 1U);
    REQUIRE_CLOSE(v.Get<float>(), 2.5f, 0.0001f);
    REQUIRE(v.TryGet<int>() == nullptr);
    REQUIRE(v.TryGet<float>() != nullptr);

    v.Reset();
    REQUIRE(!v.HasValue());
}

TEST_CASE("Variant copy and move") {
    ResetTrackerCounts();
    {
        using TTrackedVar = TVariant<FTracker, int>;
        TTrackedVar v;
        v.Emplace<FTracker>(7);
        REQUIRE_EQ(FTracker::ctorCount, 1);
        REQUIRE_EQ(FTracker::copyCount, 0);
        REQUIRE_EQ(FTracker::moveCount, 0);

        TTrackedVar copy(v);
        REQUIRE(copy.Is<FTracker>());
        REQUIRE_EQ(copy.Get<FTracker>().value, 7);
        REQUIRE_EQ(FTracker::copyCount, 1);

        TTrackedVar moved(AltinaEngine::Move(v));
        REQUIRE(moved.Is<FTracker>());
        REQUIRE_EQ(moved.Get<FTracker>().value, 7);
        REQUIRE(!v.HasValue());
        REQUIRE_EQ(FTracker::moveCount, 1);

        copy.Reset();
        REQUIRE_EQ(FTracker::dtorCount, 2);
    }

    const int totalConstructs =
        FTracker::ctorCount + FTracker::copyCount + FTracker::moveCount;
    REQUIRE_EQ(FTracker::dtorCount, totalConstructs);
}
