#include "TestHarness.h"

#include "Platform/PlatformFileSystem.h"
#include "Utility/Filesystem/Path.h"

namespace {
    using AltinaEngine::TChar;
    using AltinaEngine::Core::Platform::GetPathSeparator;
    using AltinaEngine::Core::Platform::IsPathSeparator;
    using AltinaEngine::Core::Utility::Filesystem::FPath;
    namespace Container = AltinaEngine::Core::Container;
    using Container::FString;
    using Container::FStringView;

    auto EqualsLiteral(FStringView value, const TChar* expected) -> bool {
        return value == FStringView(expected);
    }
} // namespace

TEST_CASE("Path empty basics") {
    const FPath empty;
    REQUIRE(empty.IsEmpty());
    REQUIRE(empty.ParentPath().IsEmpty());
    REQUIRE(empty.Filename().IsEmpty());
    REQUIRE(empty.Extension().IsEmpty());
    REQUIRE(empty.Stem().IsEmpty());
}

TEST_CASE("Path filename extension stem") {
    const FPath path(TEXT("Root/Dir/File.txt"));
    REQUIRE(EqualsLiteral(path.Filename(), TEXT("File.txt")));
    REQUIRE(EqualsLiteral(path.Extension(), TEXT(".txt")));
    REQUIRE(EqualsLiteral(path.Stem(), TEXT("File")));
    REQUIRE(EqualsLiteral(path.ParentPath().ToView(), TEXT("Root/Dir")));
}

TEST_CASE("Path replace extension") {
    const FPath path(TEXT("Root/Dir/File.txt"));
    const FPath replaced = path.ReplaceExtension(TEXT(".bin"));
    REQUIRE(EqualsLiteral(replaced.ToView(), TEXT("Root/Dir/File.bin")));

    const FPath replacedNoDot = path.ReplaceExtension(TEXT("bin"));
    REQUIRE(EqualsLiteral(replacedNoDot.ToView(), TEXT("Root/Dir/File.bin")));

    const FPath removed = path.ReplaceExtension(TEXT(""));
    REQUIRE(EqualsLiteral(removed.ToView(), TEXT("Root/Dir/File")));
}

TEST_CASE("Path append component") {
    FPath path(TEXT("Root/Dir"));
    path /= TEXT("File.txt");

    FString expected(TEXT("Root/Dir"));
    if (!IsPathSeparator(expected[expected.Length() - 1])) {
        expected.Append(GetPathSeparator());
    }
    expected.Append(TEXT("File.txt"));

    REQUIRE(path.GetString() == expected);
}

TEST_CASE("Path trailing separator") {
    const FPath path(TEXT("Root/Dir/"));
    REQUIRE(path.Filename().IsEmpty());
    REQUIRE(EqualsLiteral(path.ParentPath().ToView(), TEXT("Root/Dir")));
}

TEST_CASE("Path absolute detection") {
#if AE_PLATFORM_WIN
    const FPath abs(TEXT("C:\\Root\\File.txt"));
    REQUIRE(abs.IsAbsolute());
    REQUIRE(!FPath(TEXT("Root\\File.txt")).IsAbsolute());
#else
    const FPath abs(TEXT("/Root/File.txt"));
    REQUIRE(abs.IsAbsolute());
    REQUIRE(!FPath(TEXT("Root/File.txt")).IsAbsolute());
#endif
}

TEST_CASE("Path normalization") {
    const FPath path(TEXT("Root/Dir/../File.txt"));
    const FPath normalized = path.Normalized();

    FString     expected(TEXT("Root"));
    expected.Append(GetPathSeparator());
    expected.Append(TEXT("File.txt"));

    REQUIRE(normalized.GetString() == expected);
}
