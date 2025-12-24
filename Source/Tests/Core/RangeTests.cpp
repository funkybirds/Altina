#include "TestHarness.h"

#include "Algorithm/Range.h"
#include "Container/Span.h"

using namespace AltinaEngine;
using namespace AltinaEngine::Core::Container;
using namespace AltinaEngine::Core::Algorithm;

TEST_CASE("Range algorithms basic")
{
    int arr[] = {1, 3, 2, 5, 4};
    TSpan<int> s(arr);

    auto maxIt = MaxElement(s);
    REQUIRE(maxIt != s.end());
    REQUIRE_EQ(*maxIt, 5);

    auto minIt = MinElement(s);
    REQUIRE(minIt != s.end());
    REQUIRE_EQ(*minIt, 1);

    bool any_gt4 = AnyOf(s, [](int v){ return v > 4; });
    REQUIRE(any_gt4);

    bool all_pos = AllOf(s, [](int v){ return v > 0; });
    REQUIRE(all_pos);

    bool none_neg = NoneOf(s, [](int v){ return v < 0; });
    REQUIRE(none_neg);

    int sortedArr[] = {1,2,3,4};
    TSpan<int> ss(sortedArr);
    REQUIRE(IsSorted(ss));
    REQUIRE(!IsSorted(s));
}
