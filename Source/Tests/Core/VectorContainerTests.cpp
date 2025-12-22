#include "TestHarness.h"

#include "Container/Vector.h"
#include "Types/Concepts.h"

using namespace AltinaEngine;
using namespace AltinaEngine::Core::Container;

// Compile-time checks for TVector iterators and random access traits
using TVec       = TVector<int>;
using TVecIter   = TVec::iterator;
using TVecCIter  = TVec::const_iterator;

static_assert(IReadableIterator<TVecIter>);
static_assert(IWritableIterator<TVecIter>);
static_assert(IRandomAccessIterator<TVecIter>);

static_assert(IReadableIterator<TVecCIter>);
static_assert(IRandomAccessIterator<TVecCIter>);

TEST_CASE("TVector - push back and access")
{
    TVector<int> vec;
    REQUIRE(vec.IsEmpty());

    vec.PushBack(1);
    vec.PushBack(2);
    vec.PushBack(3);

    REQUIRE(vec.Size() == 3);
    REQUIRE(vec[0] == 1);
    REQUIRE(vec[1] == 2);
    REQUIRE(vec[2] == 3);

    REQUIRE(vec.Front() == 1);
    REQUIRE(vec.Back() == 3);
}

TEST_CASE("TVector - reserve and resize")
{
    TVector<int> vec;
    vec.Reserve(10);
    REQUIRE(vec.Capacity() >= 10);

    vec.Resize(5);
    REQUIRE(vec.Size() == 5);

    for (usize i = 0; i < vec.Size(); ++i)
    {
        vec[i] = static_cast<int>(i);
    }

    vec.Resize(3);
    REQUIRE(vec.Size() == 3);
    REQUIRE(vec[0] == 0);
    REQUIRE(vec[1] == 1);
    REQUIRE(vec[2] == 2);
}

TEST_CASE("TVector - copy and move semantics")
{
    TVector<int> vec;
    vec.PushBack(10);
    vec.PushBack(20);

    TVector<int> copy = vec;
    REQUIRE(copy.Size() == 2);
    REQUIRE(copy[0] == 10);
    REQUIRE(copy[1] == 20);

    TVector<int> moved = std::move(vec);
    REQUIRE(moved.Size() == 2);
    REQUIRE(moved[0] == 10);
    REQUIRE(moved[1] == 20);
}
