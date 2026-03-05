#include "TestHarness.h"

#include "Container/Tree/RbTree.h"

using namespace AltinaEngine::Core::Container;

namespace {
    struct FEntry {
        int Key     = 0;
        int Payload = 0;
    };

    struct FKeyOfEntry {
        [[nodiscard]] auto operator()(const FEntry& entry) const noexcept -> int {
            return entry.Key;
        }
    };

    using FTree = TRbTree<int, FEntry, FKeyOfEntry>;
} // namespace

TEST_CASE("TRbTree - insert find and in-order iteration") {
    FTree tree;

    auto  r0 = tree.InsertUnique(FEntry{ 3, 30 });
    auto  r1 = tree.InsertUnique(FEntry{ 1, 10 });
    auto  r2 = tree.InsertUnique(FEntry{ 4, 40 });
    auto  r3 = tree.InsertUnique(FEntry{ 2, 20 });
    auto  d0 = tree.InsertUnique(FEntry{ 3, 999 });

    REQUIRE(r0.mInserted);
    REQUIRE(r1.mInserted);
    REQUIRE(r2.mInserted);
    REQUIRE(r3.mInserted);
    REQUIRE(!d0.mInserted);
    REQUIRE_EQ(tree.Size(), 4);

    auto it = tree.Find(2);
    REQUIRE(it != tree.End());
    REQUIRE_EQ(it->Payload, 20);
    REQUIRE(!tree.Contains(9));

    int expected = 1;
    for (auto iter = tree.Begin(); iter != tree.End(); ++iter) {
        REQUIRE_EQ(iter->Key, expected);
        ++expected;
    }
    REQUIRE_EQ(expected, 5);
}

TEST_CASE("TRbTree - erase keeps search and ordering stable") {
    FTree tree;
    for (int i = 1; i <= 7; ++i) {
        REQUIRE(tree.InsertUnique(FEntry{ i, i * 10 }).mInserted);
    }

    REQUIRE(tree.Erase(1));
    REQUIRE(tree.Erase(6));
    REQUIRE(tree.Erase(4));
    REQUIRE(!tree.Erase(99));

    REQUIRE(!tree.Contains(1));
    REQUIRE(!tree.Contains(4));
    REQUIRE(!tree.Contains(6));
    REQUIRE_EQ(tree.Size(), 4);

    int expectedKeys[4] = { 2, 3, 5, 7 };
    int idx             = 0;
    for (auto it = tree.Begin(); it != tree.End(); ++it) {
        REQUIRE(idx < 4);
        REQUIRE_EQ(it->Key, expectedKeys[idx]);
        ++idx;
    }
    REQUIRE_EQ(idx, 4);
}

TEST_CASE("TRbTree - lower and upper bound") {
    FTree tree;
    REQUIRE(tree.InsertUnique(FEntry{ 10, 1 }).mInserted);
    REQUIRE(tree.InsertUnique(FEntry{ 20, 2 }).mInserted);
    REQUIRE(tree.InsertUnique(FEntry{ 30, 3 }).mInserted);

    auto lb10 = tree.LowerBound(10);
    auto lb15 = tree.LowerBound(15);
    auto lb40 = tree.LowerBound(40);
    REQUIRE(lb10 != tree.End());
    REQUIRE(lb15 != tree.End());
    REQUIRE(lb40 == tree.End());
    REQUIRE_EQ(lb10->Key, 10);
    REQUIRE_EQ(lb15->Key, 20);

    auto ub10 = tree.UpperBound(10);
    auto ub20 = tree.UpperBound(20);
    auto ub30 = tree.UpperBound(30);
    REQUIRE(ub10 != tree.End());
    REQUIRE(ub20 != tree.End());
    REQUIRE(ub30 == tree.End());
    REQUIRE_EQ(ub10->Key, 20);
    REQUIRE_EQ(ub20->Key, 30);
}
