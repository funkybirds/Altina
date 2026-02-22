#include "TestHarness.h"

#include "Math/LinAlg/LookAt.h"
#include "Math/LinAlg/Common.h"
#include "Math/Matrix.h"
#include "Math/Vector.h"

using AltinaEngine::f32;
using AltinaEngine::u32;

using namespace AltinaEngine::Core::Math;
using namespace AltinaEngine::Core::Math::LinAlg;

namespace {
    void RequireMatrixClose(const FMatrix4x4f& a, const FMatrix4x4f& b, f32 eps) {
        for (u32 r = 0U; r < 4U; ++r) {
            for (u32 c = 0U; c < 4U; ++c) {
                REQUIRE_CLOSE(a(r, c), b(r, c), eps);
            }
        }
    }
} // namespace

TEST_CASE("LookAtLH - identity at origin looking +Z") {
    const FVector3f   eye(0.0f, 0.0f, 0.0f);
    const FVector3f   target(0.0f, 0.0f, 1.0f);
    const FVector3f   up(0.0f, 1.0f, 0.0f);

    const FMatrix4x4f view     = LookAtLH(eye, target, up);
    const FMatrix4x4f expected = Identity<f32, 4>();
    RequireMatrixClose(view, expected, 1e-6f);
}

TEST_CASE("LookAtLH - translation only") {
    const FVector3f   eye(1.0f, 2.0f, 3.0f);
    const FVector3f   target(1.0f, 2.0f, 4.0f);
    const FVector3f   up(0.0f, 1.0f, 0.0f);

    const FMatrix4x4f view = LookAtLH(eye, target, up);
    REQUIRE_CLOSE(view(0, 0), 1.0f, 1e-6f);
    REQUIRE_CLOSE(view(1, 1), 1.0f, 1e-6f);
    REQUIRE_CLOSE(view(2, 2), 1.0f, 1e-6f);
    REQUIRE_CLOSE(view(0, 3), -1.0f, 1e-6f);
    REQUIRE_CLOSE(view(1, 3), -2.0f, 1e-6f);
    REQUIRE_CLOSE(view(2, 3), -3.0f, 1e-6f);
    REQUIRE_CLOSE(view(3, 3), 1.0f, 1e-6f);
}

TEST_CASE("LookAtLH - forward translation") {
    const FVector3f   eye(0.0f, 0.0f, -5.0f);
    const FVector3f   target(0.0f, 0.0f, 0.0f);
    const FVector3f   up(0.0f, 1.0f, 0.0f);

    const FMatrix4x4f view = LookAtLH(eye, target, up);
    REQUIRE_CLOSE(view(0, 0), 1.0f, 1e-6f);
    REQUIRE_CLOSE(view(1, 1), 1.0f, 1e-6f);
    REQUIRE_CLOSE(view(2, 2), 1.0f, 1e-6f);
    REQUIRE_CLOSE(view(2, 3), 5.0f, 1e-6f);
    REQUIRE_CLOSE(view(3, 3), 1.0f, 1e-6f);
}
