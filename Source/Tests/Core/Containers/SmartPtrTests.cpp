#include "TestHarness.h"

#include "Container/SmartPtr.h"

TEST_CASE("SmartPtr nullptr comparisons") {
    using namespace AltinaEngine::Core::Container;

    TOwner<int> owner;
    REQUIRE(owner == nullptr);
    REQUIRE(nullptr == owner);
    REQUIRE(!(owner != nullptr));
    REQUIRE(!(nullptr != owner));

    if (owner) {
        REQUIRE(false);
    } else {
        REQUIRE(true);
    }

    TShared<int> shared;
    REQUIRE(shared == nullptr);
    REQUIRE(nullptr == shared);
    REQUIRE(!(shared != nullptr));

    if (shared) {
        REQUIRE(false);
    } else {
        REQUIRE(true);
    }
}
