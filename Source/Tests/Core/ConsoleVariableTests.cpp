#include "TestHarness.h"

#include "../../Engine/Core/Public/Console/ConsoleVariable.h"

using namespace AltinaEngine::Core::Console;
using AltinaEngine::Core::Container::FString;

TEST_CASE("ConsoleVariable: basic register and parsing")
{
    // Register variable with integer default
    FConsoleVariable* v = FConsoleVariable::Register(FString("test.var"), FString("123"));
    REQUIRE(v);

    // Lookup should return same pointer
    FConsoleVariable* f = FConsoleVariable::Find(FString("test.var"));
    REQUIRE(f == v);

    // Parsing
    REQUIRE_EQ(v->GetInt(), 123);
    REQUIRE_CLOSE(v->GetFloat(), 123.0f, 0.001f);

    // Change to float
    v->SetFromString(FString("3.14"));
    REQUIRE_CLOSE(v->GetFloat(), 3.14f, 0.01f);

    // Boolean parsing
    v->SetFromString(FString("true"));
    REQUIRE(v->GetBool());
    v->SetFromString(FString("no"));
    REQUIRE(!v->GetBool());

    // ForEach should iterate at least this entry
    int count = 0;
    FConsoleVariable::ForEach([&](const FConsoleVariable& var) { ++count; });
    REQUIRE(count >= 1);
}
