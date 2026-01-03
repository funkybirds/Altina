#include "TestHarness.h"

#include "Reflection/Object.h"
// #include "Reflection/Reflection.h"

#include <utility>

using namespace AltinaEngine::Core::Reflection;

struct TestStruct {
    int        v;
    static int constructed;
    static int copied;
    static int destructed;

    TestStruct(int x) : v(x) { ++constructed; }
    TestStruct(const TestStruct& rhs) : v(rhs.v) { ++copied; }
    ~TestStruct() { ++destructed; }
};

int TestStruct::constructed = 0;
int TestStruct::copied      = 0;
int TestStruct::destructed  = 0;

TEST_CASE("FObject Create and As") {
    TestStruct::constructed = 0;
    TestStruct::copied      = 0;
    TestStruct::destructed  = 0;

    {
        auto obj = FObject::Create<TestStruct>(7);
        REQUIRE_EQ(obj.As<TestStruct>().v, 7);
        REQUIRE_EQ(TestStruct::constructed, 1);
    }

    // object out of scope -> destructor called
    REQUIRE_EQ(TestStruct::destructed, 1);
}

TEST_CASE("FObject copy and move semantics") {
    TestStruct::constructed = 0;
    TestStruct::copied      = 0;
    TestStruct::destructed  = 0;

    {
        std::cout << "[DBG] Create a\n";
        auto a = FObject::Create<TestStruct>(11);
        std::cout << "[DBG] After create a, constructed=" << TestStruct::constructed << "\n";
        REQUIRE_EQ(TestStruct::constructed, 1);

        std::cout << "[DBG] CreateClone b\n";
        auto b = FObject::CreateClone<TestStruct>(a.As<TestStruct>());
        std::cout << "[DBG] After clone, copied=" << TestStruct::copied << "\n";
        REQUIRE_EQ(TestStruct::copied, 1); // CreateClone invokes copy ctor

        std::cout << "[DBG] Copy c=b\n";
        auto c = b; // FObject copy ctor should copy the underlying object
        std::cout << "[DBG] After copy, copied=" << TestStruct::copied << "\n";
        REQUIRE_EQ(TestStruct::copied, 2);

        std::cout << "[DBG] Move d=move(c)\n";
        auto d = std::move(c); // move should transfer ownership without additional copies
        std::cout << "[DBG] After move\n";
        REQUIRE_EQ(d.As<TestStruct>().v, 11);
    }

    // a, b and d were destroyed
    REQUIRE_EQ(TestStruct::destructed, 3);
}
