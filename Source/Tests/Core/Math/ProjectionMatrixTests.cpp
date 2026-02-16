#include "TestHarness.h"

#include "Math/LinAlg/ProjectionMatrix.h"
#include "Math/Common.h"
#include "Math/Matrix.h"

#include <type_traits>

using namespace AltinaEngine::Core::Math::LinAlg;
using AltinaEngine::f32;
using AltinaEngine::f64;
using AltinaEngine::u32;
using AltinaEngine::Core::Math::kHalfPiF;
using AltinaEngine::Core::Math::kHalfPiD;

static_assert(std::is_base_of_v<AltinaEngine::Core::Math::TMatrix<f32, 4U, 4U>,
    FProjectionMatrixf>);
static_assert(std::is_base_of_v<AltinaEngine::Core::Math::TMatrix<f64, 4U, 4U>,
    FProjectionMatrixd>);

TEST_CASE("ProjectionMatrix - perspective fov f32") {
    const f32                fovY   = kHalfPiF;
    const f32                viewX  = 2.0f;
    const f32                viewY  = 1.0f;
    const f32                minZ   = 0.1f;
    const f32                maxZ   = 100.0f;
    const FProjectionMatrixf matrix(fovY, viewX, viewY, minZ, maxZ);

    const f32 zRange = maxZ - minZ;
    REQUIRE_CLOSE(matrix(0, 0), 0.5f, 1e-6f);
    REQUIRE_CLOSE(matrix(1, 1), 1.0f, 1e-6f);
    REQUIRE_CLOSE(matrix(2, 2), maxZ / zRange, 1e-6f);
    REQUIRE_CLOSE(matrix(2, 3), -minZ * maxZ / zRange, 1e-6f);
    REQUIRE_CLOSE(matrix(3, 2), 1.0f, 1e-6f);
    REQUIRE_CLOSE(matrix(3, 3), 0.0f, 1e-6f);
}

TEST_CASE("ProjectionMatrix - perspective fov f64") {
    const f64                fovY   = kHalfPiD;
    const f64                viewX  = 4.0;
    const f64                viewY  = 2.0;
    const f64                minZ   = 0.5;
    const f64                maxZ   = 50.0;
    const FProjectionMatrixd matrix(fovY, viewX, viewY, minZ, maxZ);

    const f64 zRange = maxZ - minZ;
    REQUIRE_CLOSE(matrix(0, 0), 0.5, 1e-12);
    REQUIRE_CLOSE(matrix(1, 1), 1.0, 1e-12);
    REQUIRE_CLOSE(matrix(2, 2), maxZ / zRange, 1e-12);
    REQUIRE_CLOSE(matrix(2, 3), -minZ * maxZ / zRange, 1e-12);
    REQUIRE_CLOSE(matrix(3, 2), 1.0, 1e-12);
    REQUIRE_CLOSE(matrix(3, 3), 0.0, 1e-12);
}
