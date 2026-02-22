#include "TestHarness.h"
#include "../../Runtime/Core/Public/Instrumentation/Instrumentation.h"
#include <cstring>

using namespace AltinaEngine::Core::Instrumentation;

// Minimal tests using the project's test harness (matches TEST_CASE style in repository)
TEST_CASE("Instrumentation: ThreadName and Counters") {
    // Thread name should be set and retrievable
    SetCurrentThreadName("UnitTestThread");
    const char* name = GetCurrentThreadName();
    REQUIRE(name);
    REQUIRE(strcmp(name, "UnitTestThread") == 0);

    // Counter operations
    IncrementCounter("test.counter", 5);
    IncrementCounter("test.counter", 3);
    auto val = GetCounterValue("test.counter");
    REQUIRE(val == 8);

    // Scoped timer records latency > 0
    {
        FScopedTimer t("test.timer");
        // small busy wait
        volatile int x = 0;
        for (int i = 0; i < 100000; ++i)
            x += i;
    }

    unsigned long long totalMs = 0, count = 0;
    GetTimingAggregate("test.timer", totalMs, count);
    REQUIRE(count >= 1);
    REQUIRE(totalMs >= 0);
}
