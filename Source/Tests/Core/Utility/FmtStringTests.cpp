#include "TestHarness.h"

#include "Utility/String/FmtString.h"

namespace {
    using AltinaEngine::u32;
    using AltinaEngine::Core::Container::FString;
    using AltinaEngine::Core::Container::FStringView;
    using AltinaEngine::Core::Utility::String::FmtString;
} // namespace

TEST_CASE("FmtString formats scalar arguments") {
    const FString formatted = FmtString(TEXT("{} {} {}"), TEXT("Frame"), 7, true);
    REQUIRE(formatted == FStringView(TEXT("Frame 7 true")));
}

TEST_CASE("FmtString formats engine string types") {
    const FString     name(TEXT("Altina"));
    const FStringView prefix    = name.SubstrView(0, 3);
    const FString     formatted = FmtString(TEXT("{}:{}"), name, prefix);

    REQUIRE(formatted == FStringView(TEXT("Altina:Alt")));
}

TEST_CASE("FmtString keeps std format features") {
    const FString formatted = FmtString(TEXT("id={:#x} {{ok}}"), static_cast<u32>(255));
    REQUIRE(formatted == FStringView(TEXT("id=0xff {ok}")));
}
