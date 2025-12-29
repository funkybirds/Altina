#include "TestHarness.h"

#include "Container/String.h"
#include "Container/StringView.h"

using namespace AltinaEngine::Core::Container;

TEST_CASE("FString constructs from literals")
{
    FString Greeting(TEXT("Hello"));
    REQUIRE_EQ(Greeting.Length(), 5U);
    REQUIRE_EQ(Greeting[0], TEXT('H'));
    REQUIRE_EQ(Greeting[4], TEXT('o'));
}

TEST_CASE("FString append and case conversion")
{
    FString Phrase(TEXT("HeLLo"));
    Phrase.Append(TEXT(" World"));
    REQUIRE_EQ(Phrase.Length(), 11U);
    REQUIRE_EQ(Phrase[10], TEXT('d'));

    const FString Lower = Phrase.ToLowerCopy();
    REQUIRE_EQ(Lower[0], TEXT('h'));
    REQUIRE_EQ(Lower[6], TEXT('w'));

    Phrase.ToUpper();
    REQUIRE_EQ(Phrase[0], TEXT('H'));
    REQUIRE_EQ(Phrase[5], TEXT(' '));
    REQUIRE_EQ(Phrase[6], TEXT('W'));
}

TEST_CASE("TStringView basic operations")
{
    FStringView Literal(TEXT("Engine"));
    REQUIRE_EQ(Literal.Length(), 6U);
    REQUIRE_EQ(Literal[1], TEXT('n'));

    auto Mid = Literal.Substring(2U, 3U);
    REQUIRE_EQ(Mid.Length(), 3U);
    REQUIRE_EQ(Mid[0], TEXT('g'));

    const FString Word(TEXT("Altina"));
    FStringView   FromView = Word.ToView();
    REQUIRE_EQ(FromView.Length(), 6U);
    REQUIRE_EQ(FromView[5], TEXT('a'));

    FStringView Implicit = Word;
    REQUIRE_EQ(Implicit[0], TEXT('A'));
}
