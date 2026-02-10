#include "TestHarness.h"

#include "Math/Physics/InertiaTensor.h"

using namespace AltinaEngine;
using namespace AltinaEngine::Core::Math;
using namespace AltinaEngine::Core::Math::Physics;

TEST_CASE("Physics - Disk moment of inertia 2D") {
    f32 r      = 2.0f;
    f32 m      = 3.0f;
    f32 expect = 0.5f * m * r * r; // 6.0
    REQUIRE_CLOSE(DiskMomentOfInertiaWrtCenter2D(r, m), expect, 1e-6f);
}

TEST_CASE("Physics - Rectangle moment of inertia 2D") {
    f32 w      = 2.0f;
    f32 h      = 3.0f;
    f32 m      = 4.0f;
    f32 expect = (1.0f / 12.0f) * m * (w * w + h * h);
    REQUIRE_CLOSE(RectMomentOfInertiaWrtCenter2D(w, h, m), expect, 1e-6f);
}

TEST_CASE("Physics - Sphere inertia tensor 3D") {
    f32         r    = 1.0f;
    f32         m    = 2.0f;
    f32         diag = (2.0f / 5.0f) * m * r * r; // 0.8
    FMatrix3x3f I    = SphereMomentOfInertiaWrtCenter3D(r, m);
    REQUIRE_CLOSE(I[0][0], diag, 1e-6f);
    REQUIRE_CLOSE(I[1][1], diag, 1e-6f);
    REQUIRE_CLOSE(I[2][2], diag, 1e-6f);
}

TEST_CASE("Physics - Cuboid inertia tensor 3D") {
    f32         w = 2.0f;
    f32         h = 3.0f;
    f32         d = 4.0f;
    f32         m = 6.0f;

    FMatrix3x3f I = CuboidMomentOfInertiaWrtCenter3D(w, h, d, m);

    f32         expect_xx = (1.0f / 12.0f) * m * (h * h + d * d); // 12.5
    f32         expect_yy = (1.0f / 12.0f) * m * (w * w + d * d); // 10.0
    f32         expect_zz = (1.0f / 12.0f) * m * (w * w + h * h); // 6.5

    REQUIRE_CLOSE(I[0][0], expect_xx, 1e-6f);
    REQUIRE_CLOSE(I[1][1], expect_yy, 1e-6f);
    REQUIRE_CLOSE(I[2][2], expect_zz, 1e-6f);
}
