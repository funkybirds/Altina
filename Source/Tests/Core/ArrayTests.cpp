#include "TestHarness.h"

#include "Container/Array.h"
#include "Types/Concepts.h"

using namespace AltinaEngine;
using namespace AltinaEngine::Core::Container;

// Compile-time checks for TArray
static_assert(IRandomReadable<TArray<int, 4>>);
static_assert(IRandomWritable<TArray<int, 4>>);

TEST_CASE("TArray - basic properties")
{
    using FArray4 = TArray<int, 4>;

    FArray4 arr{};

    REQUIRE(!FArray4::IsEmpty());
    REQUIRE(FArray4::Size() == 4);

    arr[0] = 1;
    arr[1] = 2;
    arr[2] = 3;
    arr[3] = 4;

    REQUIRE(arr[0] == 1);
    REQUIRE(arr[1] == 2);
    REQUIRE(arr[2] == 3);
    REQUIRE(arr[3] == 4);
}

TEST_CASE("TArray - iteration and algorithms")
{
    TArray<int, 4> arr{};
    int            value = 1;
    for (auto& v : arr)
    {
        v = value++;
    }

    int sum = 0;
    for (auto v : arr)
    {
        sum += v;
    }

    REQUIRE(sum == (1 + 2 + 3 + 4));
}
