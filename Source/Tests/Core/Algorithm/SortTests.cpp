#include "TestHarness.h"

#include <algorithm>
#include <vector>

#include "Algorithm/Range.h"
#include "Algorithm/Sort.h"
#include "Container/Span.h"
#include "Container/Vector.h"

using namespace AltinaEngine;
using namespace AltinaEngine::Core::Algorithm;
using namespace AltinaEngine::Core::Container;

namespace {
    struct FSortTestItem {
        i32                   Key{};
        i32                   Payload{};

        friend constexpr auto operator<(const FSortTestItem& a, const FSortTestItem& b) noexcept
            -> bool {
            return a.Key < b.Key;
        }

        friend constexpr auto operator==(const FSortTestItem& a, const FSortTestItem& b) noexcept
            -> bool {
            return a.Key == b.Key && a.Payload == b.Payload;
        }
    };

    struct FKeyGreater {
        constexpr auto operator()(const FSortTestItem& a, const FSortTestItem& b) const noexcept
            -> bool {
            return a.Key > b.Key;
        }
    };
} // namespace

TEST_CASE("Algorithm.Sort default comparator matches std::sort (i32)") {
    TVector<i32> v;
    v.Reserve(257);
    for (i32 i = 0; i < 257; ++i) {
        // Deterministic permutation of [0, 256] (gcd(37, 257) == 1).
        v.PushBack((i * 37) % 257);
    }

    std::vector<i32> baseline;
    baseline.reserve(v.Size());
    for (usize i = 0; i < v.Size(); ++i) {
        baseline.push_back(v[i]);
    }

    Sort(v);
    std::sort(baseline.begin(), baseline.end());

    REQUIRE(IsSorted(v));
    REQUIRE_EQ(v.Size(), baseline.size());
    for (usize i = 0; i < v.Size(); ++i) {
        REQUIRE_EQ(v[i], baseline[i]);
    }
}

TEST_CASE("Algorithm.Sort custom comparator matches std::sort (i32 descending)") {
    TVector<i32> v;
    v.Reserve(257);
    for (i32 i = 0; i < 257; ++i) {
        v.PushBack((i * 101) % 257);
    }

    std::vector<i32> baseline;
    baseline.reserve(v.Size());
    for (usize i = 0; i < v.Size(); ++i) {
        baseline.push_back(v[i]);
    }

    Sort(v, TGreater<>{});
    std::sort(baseline.begin(), baseline.end(), [](i32 a, i32 b) { return a > b; });

    REQUIRE(IsSorted(v, TGreater<>{}));
    REQUIRE_EQ(v.Size(), baseline.size());
    for (usize i = 0; i < v.Size(); ++i) {
        REQUIRE_EQ(v[i], baseline[i]);
    }
}

TEST_CASE("Algorithm.Sort works with TSpan over raw array") {
    i32        data[] = { 9, 1, 8, 2, 7, 3, 6, 4, 5, 0 };
    TSpan<i32> s(data);

    Sort(s);
    REQUIRE(IsSorted(s));

    for (usize i = 0; i + 1 < s.Size(); ++i) {
        REQUIRE(s[i] <= s[i + 1]);
    }
}

TEST_CASE("Algorithm.Sort matches std::sort (custom type)") {
    TVector<FSortTestItem> v;
    v.Reserve(128);

    for (i32 i = 0; i < 128; ++i) {
        // Unique keys to avoid stability assumptions.
        v.PushBack(FSortTestItem{ (i * 31) % 128, i });
    }

    std::vector<FSortTestItem> baseline;
    baseline.reserve(v.Size());
    for (usize i = 0; i < v.Size(); ++i) {
        baseline.push_back(v[i]);
    }

    Sort(v);
    std::sort(baseline.begin(), baseline.end());

    REQUIRE(IsSorted(v));
    REQUIRE_EQ(v.Size(), baseline.size());
    for (usize i = 0; i < v.Size(); ++i) {
        REQUIRE(v[i] == baseline[i]);
    }
}

TEST_CASE("Algorithm.Sort matches std::sort (custom type + custom comparator)") {
    TVector<FSortTestItem> v;
    v.Reserve(128);

    for (i32 i = 0; i < 128; ++i) {
        v.PushBack(FSortTestItem{ (i * 13) % 128, i * 7 });
    }

    std::vector<FSortTestItem> baseline;
    baseline.reserve(v.Size());
    for (usize i = 0; i < v.Size(); ++i) {
        baseline.push_back(v[i]);
    }

    Sort(v, FKeyGreater{});
    std::sort(baseline.begin(), baseline.end(), FKeyGreater{});

    REQUIRE(IsSorted(v, FKeyGreater{}));
    REQUIRE_EQ(v.Size(), baseline.size());
    for (usize i = 0; i < v.Size(); ++i) {
        REQUIRE(v[i] == baseline[i]);
    }
}
