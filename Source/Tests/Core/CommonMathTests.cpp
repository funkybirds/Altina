#include "TestHarness.h"

#include "Math/Common.h"
#include "Math/Sampling/LowDiscrepancy.h"
#include "Math/Sampling/Spherical.h"

using namespace AltinaEngine::Core::Math;
using AltinaEngine::f32;
using AltinaEngine::f64;
using AltinaEngine::i32;
using AltinaEngine::u32;

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

TEST_CASE("Math Common - Sin/Cos")
{
    // f32 checks
    REQUIRE_CLOSE(Sin<f32>(0.0F), 0.0F, 1e-6F);
    REQUIRE_CLOSE(Cos<f32>(0.0F), 1.0F, 1e-6F);

    REQUIRE_CLOSE(Sin<f32>(kHalfPiF), 1.0F, 1e-6F);
    REQUIRE_CLOSE(Cos<f32>(kHalfPiF), 0.0F, 1e-6F);

    REQUIRE_CLOSE(Sin<f32>(kPiF), 0.0F, 1e-5F);
    REQUIRE_CLOSE(Cos<f32>(kPiF), -1.0F, 1e-6F);

    // f64 checks
    REQUIRE_CLOSE(Sin<f64>(0.0), 0.0, 1e-12);
    REQUIRE_CLOSE(Cos<f64>(0.0), 1.0, 1e-12);

    REQUIRE_CLOSE(Sin<f64>(kHalfPiD), 1.0, 1e-12);
    REQUIRE_CLOSE(Cos<f64>(kHalfPiD), 0.0, 1e-12);

    REQUIRE_CLOSE(Sin<f64>(kPiD), 0.0, 1e-12);
    REQUIRE_CLOSE(Cos<f64>(kPiD), -1.0, 1e-12);
}

TEST_CASE("Math Common - Hammersley2d")
{
    const u32 N = 4;

    {
        const auto p0 = Hammersley2d(0u, N);
        REQUIRE_CLOSE(p0.X(), 0.0F, 1e-6F);
        REQUIRE_CLOSE(p0.Y(), 0.0F, 1e-6F);
    }

    {
        const auto p1 = Hammersley2d(1u, N);
        REQUIRE_CLOSE(p1.X(), 1.0F / 4.0F, 1e-6F);
        REQUIRE_CLOSE(p1.Y(), 0.5F, 1e-6F);
    }

    {
        const auto p2 = Hammersley2d(2u, N);
        REQUIRE_CLOSE(p2.X(), 2.0F / 4.0F, 1e-6F);
        REQUIRE_CLOSE(p2.Y(), 0.25F, 1e-6F);
    }

    {
        const auto p3 = Hammersley2d(3u, N);
        REQUIRE_CLOSE(p3.X(), 3.0F / 4.0F, 1e-6F);
        REQUIRE_CLOSE(p3.Y(), 0.75F, 1e-6F);
    }
}

TEST_CASE("Math Sampling - ConcentricOctahedralTransform")
{
    // center sample should map to zero vector
    const FVector2f center{ 0.5F, 0.5F };
    const auto      v = ConcentricOctahedralTransform(center);
    REQUIRE_CLOSE(v.X(), 0.0F, 1e-6F);
    REQUIRE_CLOSE(v.Y(), 0.0F, 1e-6F);
    REQUIRE_CLOSE(v.Z(), 0.0F, 1e-6F);
}
