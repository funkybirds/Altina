#include "TestHarness.h"

#include "Math/LinAlg/Common.h"

using namespace AltinaEngine::Core::Math;
using namespace AltinaEngine::Core::Math::LinAlg;

TEST_CASE("LinAlg - Inverse round-trip 2x2")
{
    TMatrix<float, 2, 2> M(float{});
    M(0, 0) = 4.0f;
    M(0, 1) = 7.0f;
    M(1, 0) = 2.0f;
    M(1, 1) = 6.0f;

    auto                 inv  = Inverse(M);
    auto                 prod = MatMul(M, inv);

    TMatrix<float, 2, 2> I = Identity<float, 2>();
    for (AltinaEngine::u32 r = 0; r < 2; ++r)
        for (AltinaEngine::u32 c = 0; c < 2; ++c)
            REQUIRE_CLOSE(prod(r, c), I(r, c), 1e-5f);
}

TEST_CASE("LinAlg - Inverse round-trip 3x3")
{
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

    auto                 inv  = Inverse(M);
    auto                 prod = MatMul(M, inv);

    TMatrix<float, 3, 3> I = Identity<float, 3>();
    for (AltinaEngine::u32 r = 0; r < 3; ++r)
        for (AltinaEngine::u32 c = 0; c < 3; ++c)
            REQUIRE_CLOSE(prod(r, c), I(r, c), 1e-5f);
}

TEST_CASE("LinAlg - Inverse round-trip 4x4")
{
    TMatrix<float, 4, 4> M = ZeroMatrix<float, 4, 4>();
    M(0, 0)                = 2.0f;
    M(1, 1)                = 3.0f;
    M(2, 2)                = 4.0f;
    M(3, 3)                = 5.0f;

    auto                 inv  = Inverse(M);
    auto                 prod = MatMul(M, inv);

    TMatrix<float, 4, 4> I = Identity<float, 4>();
    for (AltinaEngine::u32 r = 0; r < 4; ++r)
        for (AltinaEngine::u32 c = 0; c < 4; ++c)
            REQUIRE_CLOSE(prod(r, c), I(r, c), 1e-5f);
}
