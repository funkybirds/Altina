#include "TestHarness.h"

#include "View/TemporalJitter.h"

namespace {
    using AltinaEngine::f32;
    using AltinaEngine::u32;
    using AltinaEngine::RenderCore::View::ComputeHalton23JitterPx;
    using AltinaEngine::RenderCore::View::HaltonSequence;
} // namespace

TEST_CASE("RenderCore.View.TemporalJitter HaltonSequence(2) matches known values") {
    // Halton base-2: 0.5, 0.25, 0.75, 0.125, ...
    REQUIRE_CLOSE(HaltonSequence(1U, 2U), 0.5f, 1e-6f);
    REQUIRE_CLOSE(HaltonSequence(2U, 2U), 0.25f, 1e-6f);
    REQUIRE_CLOSE(HaltonSequence(3U, 2U), 0.75f, 1e-6f);
    REQUIRE_CLOSE(HaltonSequence(4U, 2U), 0.125f, 1e-6f);
}

TEST_CASE("RenderCore.View.TemporalJitter Halton23 jitter px is deterministic") {
    // sampleCount affects wrapping only; first 8 indices are fixed.
    const u32 sampleCount = 8U;

    struct FExpected {
        f32 X;
        f32 Y;
    };

    // jitterPx = (Halton2 - 0.5, Halton3 - 0.5)
    const FExpected expected[8] = {
        { 0.0f, -0.16666667f },    // (0.5, 1/3)
        { -0.25f, 0.16666667f },   // (0.25, 2/3)
        { 0.25f, -0.38888889f },   // (0.75, 1/9)
        { -0.375f, -0.05555556f }, // (0.125, 4/9)
        { 0.125f, 0.27777779f },   // (0.625, 7/9)
        { -0.125f, -0.27777779f }, // (0.375, 2/9)
        { 0.375f, 0.05555556f },   // (0.875, 5/9)
        { -0.4375f, 0.38888889f }, // (0.0625, 8/9)
    };

    for (u32 i = 0U; i < 8U; ++i) {
        const auto j = ComputeHalton23JitterPx(i, sampleCount);
        REQUIRE_CLOSE(j[0], expected[i].X, 1e-5f);
        REQUIRE_CLOSE(j[1], expected[i].Y, 1e-5f);
    }
}
