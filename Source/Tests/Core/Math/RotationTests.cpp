#include "TestHarness.h"

#include "Math/Common.h"
#include "Math/Rotation.h"

#include <cmath>

using AltinaEngine::f32;

using namespace AltinaEngine::Core::Math;

namespace {

    f32 WrapPi(f32 angle) {
        angle = std::fmod(angle + kPiF, kTwoPiF);
        if (angle < 0.0f) {
            angle += kTwoPiF;
        }
        return angle - kPiF;
    }

    bool QuatEquivalent(const FQuaternion& a, const FQuaternion& b, f32 eps) {
        const FQuaternion na  = a.Normalized();
        const FQuaternion nb  = b.Normalized();
        const f32         dot = na.x * nb.x + na.y * nb.y + na.z * nb.z + na.w * nb.w;
        return std::fabs(1.0f - std::fabs(dot)) <= eps;
    }

} // namespace

TEST_CASE("Math Rotation - Euler round-trip typical") {
    const FEulerRotator cases[] = {
        FEulerRotator(0.0f, 0.0f, 0.0f),
        FEulerRotator(kPiF * 0.25f, kPiF * 0.5f, -kPiF * 0.125f),
        FEulerRotator(-0.35f, 1.1f, 0.7f),
    };

    for (const auto& input : cases) {
        const FEulerRotator output = FEulerRotator::FromQuaternion(input.ToQuaternion());
        REQUIRE_CLOSE(WrapPi(output.pitch - input.pitch), 0.0f, 1e-4f);
        REQUIRE_CLOSE(WrapPi(output.yaw - input.yaw), 0.0f, 1e-4f);
        REQUIRE_CLOSE(WrapPi(output.roll - input.roll), 0.0f, 1e-4f);
    }
}

TEST_CASE("Math Rotation - Euler round-trip extremes") {
    const FEulerRotator cases[] = {
        FEulerRotator(kHalfPiF, 1.0f, -0.5f),
        FEulerRotator(-kHalfPiF, -1.0f, 0.5f),
        FEulerRotator(kPiF - 1e-3f, kPiF, -kPiF + 2e-3f),
        FEulerRotator(kTwoPiF * 3.0f + 0.25f, -kTwoPiF * 2.0f - 0.75f, kTwoPiF * 4.0f + 1.0f),
    };

    for (const auto& input : cases) {
        const FQuaternion   q      = input.ToQuaternion();
        const FEulerRotator output = FEulerRotator::FromQuaternion(q);
        const FQuaternion   q2     = output.ToQuaternion();
        REQUIRE(QuatEquivalent(q, q2, 1e-4f));

        REQUIRE(std::isfinite(output.pitch));
        REQUIRE(std::isfinite(output.yaw));
        REQUIRE(std::isfinite(output.roll));
    }
}
