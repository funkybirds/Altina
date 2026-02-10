#include "TestHarness.h"

#if AE_PLATFORM_WIN
    #include "Platform/Generic/GenericPlatformDecl.h"
    #include "Platform/Windows/PlatformIntrinsicWindows.h"

using namespace AltinaEngine::Core::Platform;

TEST_CASE("Platform intrinsics (Windows)") {
    STATIC_REQUIRE(PopCount32(0xFFFF0000U) == 16U);
    STATIC_REQUIRE(PopCount64(0xFFFF0000FFFF0000ULL) == 32U);
    STATIC_REQUIRE(CountLeadingZeros32(1U) == 31U);
    STATIC_REQUIRE(CountLeadingZeros64(1ULL) == 63U);
    STATIC_REQUIRE(CountTrailingZeros32(1U << 12U) == 12U);
    STATIC_REQUIRE(CountTrailingZeros64(1ULL << 36U) == 36U);

    REQUIRE(PopCount32(0xF0F0F0F0U) == 16U);
    REQUIRE(PopCount64(0xF0F0F0F0F0F0F0F0ULL) == 32U);
    REQUIRE(CountLeadingZeros32(0x04000000U) == 5U);
    REQUIRE(CountLeadingZeros64(1ULL << 40U) == 23U);
    REQUIRE(CountTrailingZeros32(0x02000000U) == 25U);
    REQUIRE(CountTrailingZeros64(1ULL << 42U) == 42U);
}
#endif // AE_PLATFORM_WIN
