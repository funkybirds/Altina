#include "TestHarness.h"

#include "Container/RobinHoodHashMap.h"
#include "Container/RobinHoodHashSet.h"

#include <random>
#include <unordered_map>
#include <cstdio>

using namespace AltinaEngine;
using namespace AltinaEngine::Core::Container;

namespace {
    struct FCollisionHash {
        auto operator()(i32 value) const -> usize { return static_cast<usize>(value & 3); }
    };
} // namespace

TEST_CASE("TRobinHoodHashMap - try emplace and find") {
    TRobinHoodHashMap<i32, i32> map;

    auto                        first = map.TryEmplace(7, 11);
    REQUIRE(first.second);
    REQUIRE(first.first != map.end());
    REQUIRE(first.first->second == 11);

    auto second = map.TryEmplace(7, 19);
    REQUIRE(!second.second);
    REQUIRE(second.first != map.end());
    REQUIRE(second.first->second == 11);

    REQUIRE(map.Num() == 1);
    REQUIRE(map.Contains(7));
    REQUIRE(!map.Contains(8));

    i32* found = map.Find(7);
    REQUIRE(found != nullptr);
    REQUIRE(*found == 11);
}

TEST_CASE("TRobinHoodHashMap - erase with backward shift") {
    TRobinHoodHashMap<i32, i32, FCollisionHash> map;
    map.Reserve(16);

    for (i32 i = 0; i < 24; ++i) {
        REQUIRE(map.Emplace(i, i * 10).second);
    }

    REQUIRE(map.Remove(5));
    REQUIRE(!map.Remove(5));
    REQUIRE(!map.Contains(5));

    for (i32 i = 0; i < 24; ++i) {
        if (i == 5) {
            continue;
        }
        i32* found = map.Find(i);
        REQUIRE(found != nullptr);
        REQUIRE(*found == i * 10);
    }
}

TEST_CASE("TRobinHoodHashSet - unique insert and remove") {
    TRobinHoodHashSet<i32> set;

    REQUIRE(set.Insert(4));
    REQUIRE(!set.Insert(4));
    REQUIRE(set.Contains(4));
    REQUIRE(set.Num() == 1);

    REQUIRE(set.Remove(4));
    REQUIRE(!set.Contains(4));
    REQUIRE(set.IsEmpty());
}

TEST_CASE("TRobinHoodHashMap - randomized parity with unordered_map") {
    TRobinHoodHashMap<u64, u32>        map;
    std::unordered_map<u64, u32>       oracle;

    std::mt19937_64                    rng(123456789ULL);
    std::uniform_int_distribution<u64> keyDist(0ULL, 4095ULL);
    std::uniform_int_distribution<u32> valDist(0U, 0xFFFFU);
    std::uniform_int_distribution<u32> opDist(0U, 99U);

    for (u32 step = 0U; step < 100000U; ++step) {
        const u32 op  = opDist(rng);
        const u64 key = keyDist(rng);

        if (op < 45U) {
            const u32 value       = valDist(rng);
            auto [itA, insertedA] = map.InsertOrAssign(key, value);
            auto [itB, insertedB] = oracle.insert_or_assign(key, value);
            REQUIRE(insertedA == insertedB);
            REQUIRE(itA != map.end());
            REQUIRE(itA->second == itB->second);
        } else if (op < 70U) {
            const usize erasedA = map.Erase(key);
            const usize erasedB = oracle.erase(key);
            REQUIRE(erasedA == erasedB);
        } else if (op < 90U) {
            auto itA = map.FindIt(key);
            auto itB = oracle.find(key);
            REQUIRE((itA == map.end()) == (itB == oracle.end()));
            if (itA != map.end() && itB != oracle.end()) {
                REQUIRE(itA->second == itB->second);
            }
        } else if (op < 95U) {
            map.Clear();
            oracle.clear();
        } else {
            map.Reserve(static_cast<usize>(keyDist(rng)));
        }

        REQUIRE(map.Num() == oracle.size());
        for (const auto& [k, v] : oracle) {
            auto it = map.FindIt(k);
            REQUIRE(it != map.end());
            REQUIRE(it->second == v);
        }
    }
}

TEST_CASE("TRobinHoodHashMap - randomized parity insert_only") {
    TRobinHoodHashMap<u64, u32>        map;
    std::unordered_map<u64, u32>       oracle;

    std::mt19937_64                    rng(987654321ULL);
    std::uniform_int_distribution<u64> keyDist(0ULL, 4095ULL);
    std::uniform_int_distribution<u32> valDist(0U, 0xFFFFU);

    for (u32 step = 0U; step < 100000U; ++step) {
        const u64 key         = keyDist(rng);
        const u32 value       = valDist(rng);
        auto [itA, insertedA] = map.InsertOrAssign(key, value);
        auto [itB, insertedB] = oracle.insert_or_assign(key, value);
        if (insertedA != insertedB || itA == map.end() || itA->second != itB->second) {
            std::printf(
                "[mismatch] step=%u key=%llu value=%u insertedA=%d insertedB=%d got=%u expect=%u\n",
                step, static_cast<unsigned long long>(key), value, insertedA ? 1 : 0,
                insertedB ? 1 : 0, itA == map.end() ? 0U : itA->second, itB->second);
            REQUIRE(false);
        }
    }

    REQUIRE(map.Num() == oracle.size());
    for (const auto& [k, v] : oracle) {
        auto it = map.FindIt(k);
        REQUIRE(it != map.end());
        REQUIRE(it->second == v);
    }
}
