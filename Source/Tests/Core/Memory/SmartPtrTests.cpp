#include <utility>

#include "TestHarness.h"

#include "Container/SmartPtr.h"

using namespace AltinaEngine::Core::Container;

TEST_CASE("TOwner release and reset") {
    auto Owner = MakeUnique<int>(42);
    REQUIRE(Owner);
    REQUIRE_EQ(*Owner, 42);

    int* RawPtr = Owner.Release();
    REQUIRE(!Owner);
    REQUIRE(RawPtr != nullptr);

    TOwner<int> Rewrapped(RawPtr);
    REQUIRE(Rewrapped);
    REQUIRE_EQ(*Rewrapped, 42);

    Rewrapped.Reset();
    REQUIRE(!Rewrapped);
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
    int Counter = 0;
    {
        TOwner<int, FCountingDeleter> Owner(new int(7), FCountingDeleter{ &Counter });
        REQUIRE_EQ(Counter, 0);
    }
    REQUIRE_EQ(Counter, 1);
}

struct FArrayCountingDeleter {
    int* CounterPtr;

    void operator()(int* Ptr) const {
        ++(*CounterPtr);
        delete[] Ptr;
    }
};

TEST_CASE("TOwner array specialization supports indexing") {
    int Counter = 0;
    {
        TOwner<int[], FArrayCountingDeleter> Owner(
            new int[3]{ 1, 2, 3 }, FArrayCountingDeleter{ &Counter });
        REQUIRE_EQ(Owner[1], 2);
        Owner[1] = 10;
        REQUIRE_EQ(Owner[1], 10);
    }
    REQUIRE_EQ(Counter, 1);
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

    TShared<int> Moved(AltinaEngine::Move(Shared));
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
    int Counter = 0;
    {
        TShared<int> Shared(new int(11), FSharedCountingDeleter{ &Counter });
        REQUIRE_EQ(Shared.UseCount(), 1U);
        REQUIRE_EQ(Counter, 0);
        {
            TShared<int> Copy = Shared;
            REQUIRE_EQ(Shared.UseCount(), 2U);
        }
        REQUIRE_EQ(Shared.UseCount(), 1U);
    }
    REQUIRE_EQ(Counter, 1);
}

TEST_CASE("AllocateShared produces owning reference") {
    TAllocator<int> Alloc;
    auto            Shared = AllocateShared<int>(Alloc, 72);
    REQUIRE(Shared);
    REQUIRE_EQ(*Shared, 72);
    REQUIRE_EQ(Shared.UseCount(), 1U);
}
