#include "TestHarness.h"

#include "Math/LinAlg/Common.h"

using namespace AltinaEngine::Core::Math;
using namespace AltinaEngine::Core::Math::LinAlg;

TEST_CASE("LinAlg - Determinant 2x2") {
    TMatrix<float, 2, 2> M(float{});
    M(0, 0) = 3.0f;
    M(0, 1) = 2.0f;
    M(1, 0) = 1.0f;
    M(1, 1) = 4.0f;

    // det = 3*4 - 2*1 = 10
    REQUIRE_CLOSE(Determinant(M), 10.0F, 1e-6F);
}

TEST_CASE("LinAlg - Determinant 3x3") {
    // |1 2 3|
    // |0 1 4|
    // |5 6 0| -> det = 1
    TMatrix<float, 3, 3> M(float{});
    M(0, 0) = 1.0f;
    M(0, 1) = 2.0f;
    M(0, 2) = 3.0f;
    M(1, 0) = 0.0f;
    M(1, 1) = 1.0f;
    M(1, 2) = 4.0f;
    M(2, 0) = 5.0f;
    M(2, 1) = 6.0f;
    M(2, 2) = 0.0f;

    REQUIRE_CLOSE(Determinant(M), 1.0F, 1e-6F);
}

TEST_CASE("LinAlg - Determinant 4x4") {
    // Upper-triangular diagonals 2,3,4,5 -> det = 2*3*4*5 = 120
    TMatrix<float, 4, 4> M = ZeroMatrix<float, 4, 4>();
    M(0, 0)                = 2.0f;
    M(1, 1)                = 3.0f;
    M(2, 2)                = 4.0f;
    M(3, 3)                = 5.0f;

    REQUIRE_CLOSE(Determinant(M), 120.0F, 1e-5F);
}
