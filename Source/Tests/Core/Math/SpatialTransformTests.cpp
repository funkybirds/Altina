#include "TestHarness.h"

#include "../../Engine/Core/Private/Math/SpatialTransform.h"

#include "Math/Common.h"
#include "Math/Matrix.h"
#include "Math/Quaternion.h"
#include "Math/Vector.h"

using namespace AltinaEngine::Core::Math;
using AltinaEngine::f32;
using AltinaEngine::u32;

namespace {

    auto ApplyTransform(const FSpatialTransform& transform, const FVector3f& point) -> FVector3f {
        const FVector3f scaled  = point * transform.Scale;
        const FVector3f rotated = transform.Rotation.RotateVector(scaled);
        return rotated + transform.Translation;
    }

    void RequireMatrixClose(const FMatrix4x4f& a, const FMatrix4x4f& b, f32 eps) {
        for (u32 r = 0U; r < 4U; ++r) {
            for (u32 c = 0U; c < 4U; ++c) {
                REQUIRE_CLOSE(a(r, c), b(r, c), eps);
            }
        }
    }

} // namespace

TEST_CASE("SpatialTransform - ToMatrix encodes TRS") {
    const FQuaternion rotation =
        FQuaternion::FromAxisAngle(FVector3f(0.0f, 0.0f, 1.0f), kHalfPiF);
    const FVector3f translation(1.0f, 2.0f, 3.0f);
    const FVector3f scale(2.0f, 3.0f, 4.0f);

    const FSpatialTransform transform(rotation, translation, scale);
    const FMatrix4x4f       m = transform.ToMatrix();

    REQUIRE_CLOSE(m(0, 0), 0.0f, 1e-6f);
    REQUIRE_CLOSE(m(0, 1), -3.0f, 1e-6f);
    REQUIRE_CLOSE(m(0, 2), 0.0f, 1e-6f);
    REQUIRE_CLOSE(m(0, 3), 1.0f, 1e-6f);

    REQUIRE_CLOSE(m(1, 0), 2.0f, 1e-6f);
    REQUIRE_CLOSE(m(1, 1), 0.0f, 1e-6f);
    REQUIRE_CLOSE(m(1, 2), 0.0f, 1e-6f);
    REQUIRE_CLOSE(m(1, 3), 2.0f, 1e-6f);

    REQUIRE_CLOSE(m(2, 0), 0.0f, 1e-6f);
    REQUIRE_CLOSE(m(2, 1), 0.0f, 1e-6f);
    REQUIRE_CLOSE(m(2, 2), 4.0f, 1e-6f);
    REQUIRE_CLOSE(m(2, 3), 3.0f, 1e-6f);

    REQUIRE_CLOSE(m(3, 0), 0.0f, 1e-6f);
    REQUIRE_CLOSE(m(3, 1), 0.0f, 1e-6f);
    REQUIRE_CLOSE(m(3, 2), 0.0f, 1e-6f);
    REQUIRE_CLOSE(m(3, 3), 1.0f, 1e-6f);
}

TEST_CASE("SpatialTransform - Multiply order applies B then A") {
    const FSpatialTransform a(
        FQuaternion::FromAxisAngle(FVector3f(0.0f, 0.0f, 1.0f), kHalfPiF),
        FVector3f(0.0f, 0.0f, 0.0f),
        FVector3f(1.0f, 1.0f, 1.0f));
    const FSpatialTransform b(
        FQuaternion::Identity(),
        FVector3f(1.0f, 0.0f, 0.0f),
        FVector3f(1.0f, 1.0f, 1.0f));

    const FVector3f point(1.0f, 0.0f, 0.0f);
    const FVector3f expected = ApplyTransform(a, ApplyTransform(b, point));

    const FSpatialTransform composed = a * b;
    const FMatrix4x4f       composedMatrix = composed.ToMatrix();
    const FVector4f         homo(point.X(), point.Y(), point.Z(), 1.0f);
    const FVector4f         result = MatMul(composedMatrix, homo);

    REQUIRE_CLOSE(result.X(), expected.X(), 1e-6f);
    REQUIRE_CLOSE(result.Y(), expected.Y(), 1e-6f);
    REQUIRE_CLOSE(result.Z(), expected.Z(), 1e-6f);

    FSpatialTransform composedAssign = a;
    composedAssign *= b;
    RequireMatrixClose(composedAssign.ToMatrix(), composedMatrix, 1e-6f);
}
