#include "TestHarness.h"
#include "Types/Traits.h"

using namespace AltinaEngine;

TEST_CASE("Comparator objects behave correctly")
{
    REQUIRE(TLess<>{}(3, 4));
    REQUIRE(TGreater<int>{}(5, 2));
    REQUIRE(TEqual<int>{}(7, 7));
}
