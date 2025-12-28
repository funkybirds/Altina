#include "TestHarness.h"
#include "Types/Meta.h"
#include <string>

using namespace AltinaEngine::Core::TypeMeta;

TEST_CASE("TMetaTypeInfo_int")
{
    REQUIRE(TMetaTypeInfo<int>::kName.Length() > 0);
    REQUIRE_EQ(TMetaTypeInfo<int>::kDefaultConstructible, true);
}

TEST_CASE("TMetaTypeInfo_hash_diff") { REQUIRE(TMetaTypeInfo<int>::kHash != TMetaTypeInfo<float>::kHash); }

TEST_CASE("TMetaTypeInfo_custom_no_default")
{
    struct NoDefault
    {
        NoDefault(int) {}
    };

    REQUIRE_EQ(TMetaTypeInfo<NoDefault>::kDefaultConstructible, false);
    REQUIRE(TMetaTypeInfo<NoDefault>::kName.Length() > 0);
}

TEST_CASE("TMetaTypeInfo_name_int")
{
    auto name = TMetaTypeInfo<int>::kName;
    REQUIRE(name.Length() >= 3);
    REQUIRE(name[0] == 'i');
    REQUIRE(name[1] == 'n');
    REQUIRE(name[2] == 't');
}

TEST_CASE("TMetaTypeInfo_name_custom_contains")
{
    struct NoDefault2
    {
        NoDefault2(int) {}
    };

    auto        name      = TMetaTypeInfo<NoDefault2>::kName;
    const char* needle    = "NoDefault2";
    const auto  needleLen = 10u;
    bool        found     = false;
    for (AltinaEngine::usize i = 0; i + needleLen <= name.Length(); ++i)
    {
        bool ok = true;
        for (AltinaEngine::usize j = 0; j < needleLen; ++j)
        {
            if (name[i + j] != needle[j])
            {
                ok = false;
                break;
            }
        }
        if (ok)
        {
            found = true;
            break;
        }
    }
    REQUIRE(found);
}
