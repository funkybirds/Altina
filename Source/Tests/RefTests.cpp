#include "TestHarness.h"
#include "Container/Ref.h"
#include "Reflection/Reflection.h"
#include <type_traits>

using namespace AltinaEngine::Core::Container;

TEST_CASE("Ref.Basic")
{
    static_assert(std::is_same_v<TRef<int>::Type, int>, "TRef<int>::Type should be int");

    int  value = 10;

    // MakeRef helper
    auto r1 = MakeRef(value);
    REQUIRE_EQ(r1.get(), 10);

    // Mutate through reference wrapper
    r1.get() = 20;
    REQUIRE_EQ(value, 20);

    // operator T& conversion
    int& conv = r1;
    REQUIRE_EQ(conv, 20);

    // From helper and call operator
    auto r2 = TRef<int>::From(value);
    REQUIRE_EQ(r2(), 20);

    // Direct construction and copy semantics
    TRef<int> r3(value);
    REQUIRE_EQ(r3.get(), 20);

    auto r4 = r3; // copy should refer to same int
    r4()    = 33;
    REQUIRE_EQ(value, 33);
}
