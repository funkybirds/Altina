#include "TestHarness.h"

#include "Container/String.h"
#include "Container/StringView.h"
#include <unordered_map>
#include <unordered_set>

using namespace AltinaEngine::Core::Container;

TEST_CASE("FString constructs from literals") {
    FString Greeting(TEXT("Hello"));
    REQUIRE_EQ(Greeting.Length(), 5U);
    REQUIRE_EQ(Greeting[0], TEXT('H'));
    REQUIRE_EQ(Greeting[4], TEXT('o'));
}

TEST_CASE("FString append and case conversion") {
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

TEST_CASE("TStringView basic operations") {
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

TEST_CASE("TStringView search and boundary behavior") {
    const FStringView text(TEXT("ababa"));
    REQUIRE_EQ(text.Find(FStringView(TEXT("aba"))), 0U);
    REQUIRE_EQ(text.Find(FStringView(TEXT("aba")), 1U), 2U);
    REQUIRE_EQ(text.RFind(FStringView(TEXT("aba"))), 2U);
    REQUIRE_EQ(text.RFind(FStringView(TEXT("aba")), 1U), 0U);

    REQUIRE_EQ(text.Find(TEXT('b')), 1U);
    REQUIRE_EQ(text.RFind(TEXT('b')), 3U);
    REQUIRE_EQ(text.Find(TEXT('z')), FStringView::npos);

    REQUIRE(text.StartsWith(FStringView(TEXT("ab"))));
    REQUIRE(text.EndsWith(FStringView(TEXT("ba"))));
    REQUIRE(text.StartsWith(FStringView(TEXT(""))));
    REQUIRE(text.EndsWith(FStringView(TEXT(""))));

    REQUIRE(text.Contains(FStringView(TEXT("bab"))));
    REQUIRE(!text.Contains(FStringView(TEXT("bbb"))));
    REQUIRE(text.Contains(TEXT('a')));

    REQUIRE_EQ(text.Find(FStringView(TEXT("")), 0U), 0U);
    REQUIRE_EQ(text.Find(FStringView(TEXT("")), text.Length()), text.Length());
    REQUIRE_EQ(text.Find(FStringView(TEXT("")), text.Length() + 1), FStringView::npos);

    const FStringView set(TEXT("bx"));
    REQUIRE_EQ(text.FindFirstOf(set), 1U);
    REQUIRE_EQ(text.FindLastOf(set), 3U);
    REQUIRE_EQ(text.FindFirstNotOf(FStringView(TEXT("ab"))), FStringView::npos);
    REQUIRE_EQ(text.FindLastNotOf(FStringView(TEXT("ab"))), FStringView::npos);
    REQUIRE_EQ(text.FindFirstNotOf(FStringView(TEXT("a"))), 1U);
    REQUIRE_EQ(text.FindLastNotOf(FStringView(TEXT("a"))), 3U);
}

TEST_CASE("TBasicString comparison, substrings, and concatenation") {
    FString left(TEXT("abc"));
    FString right(TEXT("abd"));
    REQUIRE(left.Compare(right.ToView()) < 0);
    REQUIRE(right.Compare(left.ToView()) > 0);
    REQUIRE_EQ(left.Compare(left.ToView()), 0);
    REQUIRE(left < right.ToView());

    const FString base(TEXT("HelloWorld"));
    const FString mid = base.Substr(5, 5);
    REQUIRE_EQ(mid.Length(), 5U);
    REQUIRE(mid == FStringView(TEXT("World")));

    const FString tail = base.Substr(5);
    REQUIRE_EQ(tail.Length(), 5U);
    REQUIRE(tail == FStringView(TEXT("World")));

    const FString empty = base.Substr(100);
    REQUIRE_EQ(empty.Length(), 0U);

    const auto view = base.SubstrView(3, 4);
    REQUIRE_EQ(view.Length(), 4U);
    REQUIRE_EQ(view[0], TEXT('l'));

    FString concat = left + FStringView(TEXT("123"));
    REQUIRE_EQ(concat.Length(), 6U);
    REQUIRE(concat.EndsWith(FStringView(TEXT("123"))));
}

TEST_CASE("TBasicString append overlaps and null termination") {
    FString text(TEXT("abc"));
    const auto view = text.SubstrView(1, 2);
    text.Append(view);
    REQUIRE(text == FStringView(TEXT("abcbc")));

    const FString empty;
    const auto* emptyCStr = empty.CStr();
    REQUIRE(emptyCStr != nullptr);
    REQUIRE_EQ(emptyCStr[0], TEXT('\0'));

    FString payload(TEXT("data"));
    const auto* cstr = payload.CStr();
    REQUIRE_EQ(cstr[4], TEXT('\0'));
}

TEST_CASE("TBasicString number append and hashing") {
    FNativeString num;
    num.AppendNumber(42);
    REQUIRE(num == FNativeStringView("42"));

    FNativeString num2 = FNativeString::ToString(7);
    REQUIRE(num2 == FNativeStringView("7"));

    std::unordered_map<FNativeString, int> map;
    map[FNativeString("alpha")] = 11;
    REQUIRE_EQ(map[FNativeString("alpha")], 11);

    std::unordered_set<FNativeStringView> set;
    set.insert(FNativeStringView("beta"));
    REQUIRE(set.find(FNativeStringView("beta")) != set.end());
}
