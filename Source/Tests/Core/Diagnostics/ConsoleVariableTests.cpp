#include "TestHarness.h"

#include "../../Runtime/Core/Public/Console/ConsoleVariable.h"

using namespace AltinaEngine::Core::Console;

TEST_CASE("ConsoleVariable: basic register and parsing") {
    // Register variable with integer default
    FConsoleVariable* v = FConsoleVariable::Register(TEXT("test.int"), 123);
    REQUIRE(v);

    // Lookup should return same pointer
    FConsoleVariable* f = FConsoleVariable::Find(FString(TEXT("test.int")));
    REQUIRE(f == v);

    // Parsing
    REQUIRE_EQ(v->GetValue<int>(), 123);
    REQUIRE_CLOSE(v->GetValue<float>(), 123.0f, 0.001f);
    v->SetFromString(FString(TEXT("456")));
    REQUIRE_EQ(v->GetValue<int>(), 456);

    // Float parsing
    FConsoleVariable* fvar = FConsoleVariable::Register(TEXT("test.float"), 1.0f);
    REQUIRE(fvar);
    fvar->SetFromString(FString(TEXT("3.14")));
    REQUIRE_CLOSE(fvar->GetValue<float>(), 3.14f, 0.01f);

    // Boolean parsing
    FConsoleVariable* bvar = FConsoleVariable::Register(TEXT("test.bool"), false);
    REQUIRE(bvar);
    bvar->SetFromString(FString(TEXT("true")));
    REQUIRE(bvar->GetValue<bool>());
    bvar->SetFromString(FString(TEXT("no")));
    REQUIRE(!bvar->GetValue<bool>());

    // String parsing
    FConsoleVariable* svar = FConsoleVariable::Register(TEXT("test.str"), FString(TEXT("hello")));
    REQUIRE(svar);
    auto view = svar->GetString().ToView();
    REQUIRE_EQ(view.Length(), 5U);
    REQUIRE_EQ(view[0], TEXT("hello")[0]);
    REQUIRE_EQ(view[4], TEXT("hello")[4]);

    // ForEach should iterate at least this entry
    int count = 0;
    FConsoleVariable::ForEach([&](const FConsoleVariable&) { ++count; });
    REQUIRE(count >= 4);
}
