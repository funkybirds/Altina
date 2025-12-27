#include "TestHarness.h"

#include "Math/Matrix.h"
#include "Math/Vector.h"

using namespace AltinaEngine::Core::Math;

TEST_CASE("Matrix - element access and fill")
{
    FMatrix3x3f m(3.0f);
    REQUIRE_CLOSE(m(0, 0), 3.0F, 1e-6F);
    REQUIRE_CLOSE(m(1, 1), 3.0F, 1e-6F);
    REQUIRE_CLOSE(m(2, 2), 3.0F, 1e-6F);

    m(1, 2) = 4.5F;
    REQUIRE_CLOSE(m(1, 2), 4.5F, 1e-6F);
}

TEST_CASE("Matrix - elementwise ops")
{
    FMatrix3x3f A(1.0f);
    FMatrix3x3f B(2.0f);

    auto        C = A + B;
    REQUIRE_CLOSE(C(0, 0), 3.0F, 1e-6F);

    auto D = B - A;
    REQUIRE_CLOSE(D(2, 1), 1.0F, 1e-6F);

    auto E = A * B; // element-wise multiply
    REQUIRE_CLOSE(E(0, 2), 2.0F, 1e-6F);

    auto F = B / A;
    REQUIRE_CLOSE(F(1, 1), 2.0F, 1e-6F);
}

TEST_CASE("Matrix - multiply vector")
{
    FMatrix3x3f I(0.0f);
    I(0, 0) = 1.0F;
    I(1, 1) = 1.0F;
    I(2, 2) = 1.0F;

    FVector3f v{ 1.0F, 2.0F, 3.0F };
    auto      out = MatMul(I, v);
    REQUIRE_CLOSE(out[0U], 1.0F, 1e-6F);
    REQUIRE_CLOSE(out[1U], 2.0F, 1e-6F);
    REQUIRE_CLOSE(out[2U], 3.0F, 1e-6F);

    // Transpose sanity: transpose of identity is identity
    auto It = Transpose(I);
    REQUIRE_CLOSE(It(0, 0), 1.0F, 1e-6F);
    REQUIRE_CLOSE(It(1, 1), 1.0F, 1e-6F);
    REQUIRE_CLOSE(It(2, 2), 1.0F, 1e-6F);
}
