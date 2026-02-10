#include "TestHarness.h"

#include "Types/NonCopyable.h"
#include "Types/CheckedCast.h"

TEST_CASE("NonCopyable and CheckedCast basics") {
    // Verify NonCopyable types are instantiable
    {
        AltinaEngine::FNonCopyableClass  a;
        AltinaEngine::FNonCopyableStruct s;
        REQUIRE(true);
    }

    // Polymorphic conversion test for CheckedCast
    struct Base {
        virtual ~Base() = default;
    };
    struct Derived : Base {
        int x = 42;
    };

    Derived d;
    Base*   b = &d;

    auto*   casted = AltinaEngine::CheckedCast<Derived*>(b);
    REQUIRE(casted != nullptr);
    REQUIRE_EQ(casted->x, 42);

    // Rvalue static conversion path
    int  i  = 5;
    long li = AltinaEngine::CheckedCast<long>(i);
    REQUIRE_EQ(li, 5);
}
