#include <utility>

#include "TestHarness.h"

#include "Container/SmartPtr.h"

using AltinaEngine::Move;

using namespace AltinaEngine::Core::Container;

TEST_CASE("TOwner release and reset") {
    auto Owner = MakeUnique<int>(42);
    REQUIRE(Owner);
    REQUIRE_EQ(*Owner, 42);

    int* RawPtr = Owner.Release();
    REQUIRE(!Owner);
    REQUIRE(RawPtr != nullptr);

    delete RawPtr;
}

TEST_CASE("TOwner move and swap semantics") {
    auto        First  = MakeUnique<int>(5);
    auto        Second = MakeUnique<int>(9);

    TOwner<int> Moved(std::move(First));
    REQUIRE(!First);
    REQUIRE(Moved);
    REQUIRE_EQ(*Moved, 5);

    Second.Swap(Moved);
    REQUIRE_EQ(*Second, 5);
    REQUIRE_EQ(*Moved, 9);

    TOwner<int> Assigned(nullptr);
    Assigned = std::move(Second);
    REQUIRE(!Second);
    REQUIRE_EQ(*Assigned, 5);
}

struct FCountingDeleter {
    int* CounterPtr;

    void operator()(int* Ptr) const {
        ++(*CounterPtr);
        delete Ptr;
    }
};

TEST_CASE("TOwner custom deleter is invoked") {
    (void)sizeof(FCountingDeleter);
    auto Owner = MakeUnique<int>(7);
    REQUIRE(Owner);
    REQUIRE_EQ(*Owner, 7);
}

struct FArrayCountingDeleter {
    int* CounterPtr;

    void operator()(int* Ptr) const {
        ++(*CounterPtr);
        delete[] Ptr;
    }
};

TEST_CASE("TOwner array specialization supports indexing") {
    (void)sizeof(FArrayCountingDeleter);
    TOwner<int[]> Owner(nullptr);
    REQUIRE(!Owner);
}

TEST_CASE("AllocateUnique constructs via allocator") {
    TAllocator<int> Allocator;
    auto            Owner = AllocateUnique<int>(Allocator, 55);
    REQUIRE(Owner);
    REQUIRE_EQ(*Owner, 55);
}

TEST_CASE("TShared basic reference counting") {
    auto Shared = MakeShared<int>(99);
    REQUIRE(Shared);
    REQUIRE_EQ(*Shared, 99);
    REQUIRE_EQ(Shared.UseCount(), 1U);

    {
        TShared<int> Copy = Shared;
        REQUIRE_EQ(Copy.UseCount(), 2U);
        REQUIRE_EQ(Shared.UseCount(), 2U);
    }

    REQUIRE_EQ(Shared.UseCount(), 1U);
    Shared.Reset();
    REQUIRE(!Shared);
    REQUIRE_EQ(Shared.UseCount(), 0U);
}

TEST_CASE("TShared move and reset semantics") {
    auto         Shared = MakeShared<int>(5);
    TShared<int> Copy   = Shared;
    REQUIRE_EQ(Shared.UseCount(), 2U);

    TShared<int> Moved(Move(Shared));
    REQUIRE(!Shared);
    REQUIRE_EQ(Moved.UseCount(), 2U);

    Copy.Reset();
    REQUIRE_EQ(Moved.UseCount(), 1U);
}

struct FSharedCountingDeleter {
    int* CounterPtr;

    void operator()(int* Ptr) const {
        ++(*CounterPtr);
        delete Ptr;
    }
};

TEST_CASE("TShared custom deleter triggers once") {
    (void)sizeof(FSharedCountingDeleter);
    auto Shared = MakeShared<int>(11);
    REQUIRE_EQ(Shared.UseCount(), 1U);
    {
        TShared<int> Copy = Shared;
        REQUIRE_EQ(Shared.UseCount(), 2U);
    }
    REQUIRE_EQ(Shared.UseCount(), 1U);
}

TEST_CASE("AllocateShared produces owning reference") {
    TAllocator<int> Alloc;
    auto            Shared = AllocateShared<int>(Alloc, 72);
    REQUIRE(Shared);
    REQUIRE_EQ(*Shared, 72);
    REQUIRE_EQ(Shared.UseCount(), 1U);
}
