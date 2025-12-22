#include "TestHarness.h"

#include "Math/Common.h"

using namespace AltinaEngine::Core::Math;
using AltinaEngine::i32;
using AltinaEngine::u32;
using AltinaEngine::f32;
using AltinaEngine::f64;

// Compile-time sanity for identical-type constraints
static_assert(Max(1, 2, 3) == 3);
static_assert(Min(1, 2, 3) == 1);
static_assert(Max(1.0F, -2.0F, 0.5F) == 1.0F);
static_assert(Min(1.0, -2.0, 0.5) == -2.0);
// Concepts ensure these invalid mixes are rejected at compile time
static_assert(!AltinaEngine::ISameAsAll<int, double>);
static_assert(!AltinaEngine::ISameAsAll<float, int>);

static_assert(Floor<i32>(1.9F) == 1);
static_assert(Floor<i32>(-1.1F) == -2);
static_assert(Ceil<i32>(1.1F) == 2);
static_assert(Ceil<i32>(-1.1F) == -1);
// Floor/Ceil require signed integral destination types
static_assert(!AltinaEngine::ISignedIntegral<u32>);

static_assert(Lerp<f64>(0.0, 10.0, 0.25) == 2.5);
static_assert(Clamp(5, 0, 10) == 5);
static_assert(Clamp(-1, 0, 10) == 0);
static_assert(Clamp(42, 0, 10) == 10);

TEST_CASE("Math Common - MaxMin runtime")
{
    REQUIRE(Max(3, 7, 5) == 7);
    REQUIRE(Min(3, 7, 5) == 3);

    REQUIRE_CLOSE(Max(1.5F, 1.6F, 1.4F), 1.6F, 1e-6F);
    REQUIRE_CLOSE(Min(-1.5F, -1.6F, -1.4F), -1.6F, 1e-6F);
}

TEST_CASE("Math Common - Floor/Ceil")
{
    REQUIRE(Floor<i32>(3.9F) == 3);
    REQUIRE(Floor<i32>(-3.1F) == -4);

    REQUIRE(Ceil<i32>(3.1F) == 4);
    REQUIRE(Ceil<i32>(-3.9F) == -3);
}

TEST_CASE("Math Common - Lerp")
{
    REQUIRE_CLOSE(Lerp<f32>(0.0F, 10.0F, 0.5F), 5.0F, 1e-6F);
    REQUIRE_CLOSE(Lerp<f64>(-2.0, 2.0, 0.25), -1.0, 1e-12);
}

TEST_CASE("Math Common - Clamp")
{
    REQUIRE(Clamp(5, 0, 10) == 5);
    REQUIRE(Clamp(-5, 0, 10) == 0);
    REQUIRE(Clamp(15, 0, 10) == 10);

    REQUIRE_CLOSE(Clamp(1.5F, 0.0F, 1.0F), 1.0F, 1e-6F);
}
