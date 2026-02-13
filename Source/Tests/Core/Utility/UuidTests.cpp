#include "TestHarness.h"

#include "Utility/Uuid.h"

#include <string_view>

namespace {
    namespace Container = AltinaEngine::Core::Container;
    using AltinaEngine::FUuid;
    using AltinaEngine::u8;
    using Container::FNativeString;
    using Container::FNativeStringView;

    auto EqualsLiteral(const FNativeString& value, const char* expected) -> bool {
        const std::string_view view(value.GetData(), value.Length());
        return view == std::string_view(expected);
    }
} // namespace

TEST_CASE("Uuid nil basics") {
    const FUuid nil = FUuid::Nil();
    REQUIRE(nil.IsNil());

    const auto text = nil.ToNativeString();
    REQUIRE_EQ(text.Length(), FUuid::kStringLength);
    REQUIRE(EqualsLiteral(text, "00000000-0000-0000-0000-000000000000"));
}

TEST_CASE("Uuid parse and format") {
    const char* hyphenated = "550e8400-e29b-41d4-a716-446655440000";
    const char* compact    = "550e8400e29b41d4a716446655440000";
    const char* uppercase  = "550E8400-E29B-41D4-A716-446655440000";

    FUuid       a;
    REQUIRE(FUuid::TryParse(FNativeStringView(hyphenated), a));

    FUuid b;
    REQUIRE(FUuid::TryParse(FNativeStringView(compact), b));

    FUuid c;
    REQUIRE(FUuid::TryParse(FNativeStringView(uppercase), c));

    REQUIRE(a == b);
    REQUIRE(a == c);

    const auto formatted = a.ToNativeString();
    REQUIRE(EqualsLiteral(formatted, hyphenated));
}

TEST_CASE("Uuid parse rejects invalid input") {
    FUuid out = FUuid::New();

    REQUIRE(!FUuid::TryParse(FNativeStringView("not-a-uuid"), out));
    REQUIRE(!FUuid::TryParse(FNativeStringView("550e8400-e29b-41d4-a716-44665544000Z"), out));
    REQUIRE(!FUuid::TryParse(FNativeStringView("550e8400-e29b-41d4-a716-446655440000-"), out));
}

TEST_CASE("Uuid new sets version and variant") {
    const FUuid uuid = FUuid::New();
    REQUIRE(!uuid.IsNil());

    const auto& bytes = uuid.GetBytes();
    REQUIRE_EQ(static_cast<u8>(bytes[6] & 0xF0), static_cast<u8>(0x40));
    REQUIRE_EQ(static_cast<u8>(bytes[8] & 0xC0), static_cast<u8>(0x80));

    const auto text = uuid.ToNativeString();
    FUuid      roundTrip;
    REQUIRE(FUuid::TryParse(FNativeStringView(text.GetData(), text.Length()), roundTrip));
    REQUIRE(uuid == roundTrip);
}
