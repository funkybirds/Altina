#include "TestHarness.h"

#include "Memory/ObjectPool.h"

#include <type_traits>

namespace {
    using AltinaEngine::Move;
    using AltinaEngine::Core::Memory::TObjectPoolHandle;
    using AltinaEngine::Core::Memory::TSingleThreadedObjectPool;
    using AltinaEngine::Core::Memory::TThreadSafeObjectPool;

    struct FPoolCounter {
        static int sCtorCount;
        static int sDtorCount;

        int        mValue = 0;

        FPoolCounter() { ++sCtorCount; }
        explicit FPoolCounter(int value) : mValue(value) { ++sCtorCount; }
        ~FPoolCounter() { ++sDtorCount; }
    };

    int  FPoolCounter::sCtorCount = 0;
    int  FPoolCounter::sDtorCount = 0;

    void ResetCounters() {
        FPoolCounter::sCtorCount = 0;
        FPoolCounter::sDtorCount = 0;
    }

    constexpr bool kHandleNotConvertibleToPtr =
        !std::is_convertible_v<TObjectPoolHandle<int>, int*>;
    STATIC_REQUIRE(kHandleNotConvertibleToPtr);
} // namespace

TEST_CASE("ObjectPool.SingleThreaded.AllocateDeallocate") {
    ResetCounters();

    TSingleThreadedObjectPool<FPoolCounter> pool;
    pool.Init(2);

    auto handle = pool.Allocate(7);
    REQUIRE(handle);
    REQUIRE_EQ(handle->mValue, 7);
    REQUIRE_EQ(FPoolCounter::sCtorCount, 1);

    pool.Deallocate(handle);
    REQUIRE(!handle);
    REQUIRE_EQ(FPoolCounter::sDtorCount, 1);

    pool.Deallocate(handle);
    REQUIRE_EQ(FPoolCounter::sDtorCount, 1);
}

TEST_CASE("ObjectPool.Handle.MoveOnly") {
    ResetCounters();

    TSingleThreadedObjectPool<FPoolCounter> pool;
    auto                                    handle = pool.Allocate(3);
    REQUIRE(handle);

    auto moved = Move(handle);
    REQUIRE(!handle);
    REQUIRE(moved);
    REQUIRE_EQ(moved->mValue, 3);

    pool.Deallocate(moved);
    REQUIRE_EQ(FPoolCounter::sCtorCount, 1);
    REQUIRE_EQ(FPoolCounter::sDtorCount, 1);
}

TEST_CASE("ObjectPool.ThreadSafe.Basic") {
    ResetCounters();

    TThreadSafeObjectPool<FPoolCounter> pool;
    auto                                a = pool.Allocate(11);
    auto                                b = pool.Allocate(22);

    REQUIRE(a);
    REQUIRE(b);
    REQUIRE_EQ(a->mValue, 11);
    REQUIRE_EQ(b->mValue, 22);
    REQUIRE_EQ(FPoolCounter::sCtorCount, 2);

    pool.Deallocate(a);
    pool.Deallocate(b);
    REQUIRE_EQ(FPoolCounter::sDtorCount, 2);
}
