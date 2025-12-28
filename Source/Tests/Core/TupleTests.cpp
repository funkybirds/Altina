#include "TestHarness.h"
#include "Container/Tuple.h"

using namespace AltinaEngine::Core::Container;

TEST_CASE("Tuple_basic_access_and_size")
{
    TTuple<int, double, char> t(42, 3.14159, 'x');
    REQUIRE_EQ(Get<0>(t), 42);
    REQUIRE_CLOSE(Get<1>(t), 3.14159, 1e-9);
    REQUIRE_EQ(Get<2>(t), 'x');
    REQUIRE_EQ((TTuple<int, double, char>::Size()), 3);
}

TEST_CASE("Tuple_const_and_rvalue")
{
    const TTuple<int, char> ct(7, 'a');
    REQUIRE_EQ(Get<0>(ct), 7);
    REQUIRE_EQ(Get<1>(ct), 'a');

    // rvalue access returns an rvalue reference; ensure value moves or copies as expected
    auto val = Get<0>(TTuple<int, char>(9, 'z'));
    REQUIRE_EQ(val, 9);
}
