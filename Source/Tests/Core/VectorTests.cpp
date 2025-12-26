#include "TestHarness.h"

#include "Math/Vector.h"

using namespace AltinaEngine::Core::Math;

TEST_CASE("Vector addition")
{
    FVector3f       VecA{ 1.0F, 2.0F, 3.0F };
    const FVector3f VecB{ 4.0F, 5.0F, 6.0F };
    const auto      VecAdd = VecA + VecB;
    REQUIRE_CLOSE(VecAdd[0U], 5.0F, 1e-6F);
    REQUIRE_CLOSE(VecAdd[1U], 7.0F, 1e-6F);
    REQUIRE_CLOSE(VecAdd[2U], 9.0F, 1e-6F);
}

TEST_CASE("Vector subtraction")
{
    FVector3f       VecA{ 1.0F, 2.0F, 3.0F };
    const FVector3f VecB{ 4.0F, 5.0F, 6.0F };
    const auto      VecSub = VecB - VecA;
    REQUIRE_CLOSE(VecSub[0U], 3.0F, 1e-6F);
    REQUIRE_CLOSE(VecSub[1U], 3.0F, 1e-6F);
    REQUIRE_CLOSE(VecSub[2U], 3.0F, 1e-6F);
}

TEST_CASE("Vector elementwise multiplication")
{
    FVector3f       VecA{ 1.0F, 2.0F, 3.0F };
    const FVector3f VecC{ 2.0F, 3.0F, 4.0F };
    const auto      VecMul = VecA * VecC;
    REQUIRE_CLOSE(VecMul[0U], 2.0F, 1e-6F);
    REQUIRE_CLOSE(VecMul[1U], 6.0F, 1e-6F);
    REQUIRE_CLOSE(VecMul[2U], 12.0F, 1e-6F);
}

TEST_CASE("Vector compound ops and division")
{
    const FVector3f VecD{ 2.0F, 4.0F, 8.0F };
    const FVector3f VecDivisor{ 2.0F, 2.0F, 4.0F };
    const auto      VecDiv = VecD / VecDivisor;
    REQUIRE_CLOSE(VecDiv[0U], 1.0F, 1e-6F);
    REQUIRE_CLOSE(VecDiv[1U], 2.0F, 1e-6F);
    REQUIRE_CLOSE(VecDiv[2U], 2.0F, 1e-6F);

    FVector3i       RuntimeA{ 1, 2, 3 };
    const FVector3i RuntimeB{ 4, 5, 6 };
    RuntimeA += RuntimeB;
    REQUIRE_EQ(RuntimeA[0U], 5);
    REQUIRE_EQ(RuntimeA[1U], 7);
    REQUIRE_EQ(RuntimeA[2U], 9);

    RuntimeA -= RuntimeB;
    REQUIRE_EQ(RuntimeA[0U], 1);
    REQUIRE_EQ(RuntimeA[1U], 2);
    REQUIRE_EQ(RuntimeA[2U], 3);

    const FVector3i Scale{ 2, 3, 4 };
    RuntimeA *= Scale;
    REQUIRE_EQ(RuntimeA[0U], 2);
    REQUIRE_EQ(RuntimeA[1U], 6);
    REQUIRE_EQ(RuntimeA[2U], 12);

    RuntimeA /= Scale;
    REQUIRE_EQ(RuntimeA[0U], 1);
    REQUIRE_EQ(RuntimeA[1U], 2);
    REQUIRE_EQ(RuntimeA[2U], 3);
}
