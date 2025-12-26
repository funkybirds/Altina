#include "TestHarness.h"

#include "Container/Function.h"

using namespace AltinaEngine::Core::Container;

TEST_CASE("TFunction basic invoke and copy/move")
{
    // Simple add
    printf("TEST: start basic invoke and copy/move\n");
    TFunction<int(int, int)> add = [](int a, int b) { return a + b; };
    REQUIRE_EQ(add(2, 3), 5);
    TFunction<int(int, int)> mv = std::move(add);
    // ensure moved-to callable works
    REQUIRE_EQ(mv(4, 5), 9);
}

TEST_CASE("TFunction captures and move-only callable")
{
    int              cap   = 7;
    TFunction<int()> capFn = [cap]() { return cap; };
    REQUIRE(capFn);
    REQUIRE_EQ(capFn(), 7);

    // Move-only callable
    struct MoveOnly
    {
        std::unique_ptr<int> p;
        MoveOnly(int v) : p(new int(v)) {}
        MoveOnly(MoveOnly&&)      = default;
        MoveOnly(const MoveOnly&) = delete;
        int operator()() { return *p; }
    };

    TFunction<int()> moFn(MoveOnly(42));
    REQUIRE(moFn);
    TFunction<int()> moMoved(std::move(moFn));
    REQUIRE(moMoved);
    REQUIRE_EQ(moMoved(), 42);
}
