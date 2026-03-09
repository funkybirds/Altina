#include "TestHarness.h"

#include "Math/Geometry2D.h"

using namespace AltinaEngine::Core::Math;

TEST_CASE("Geometry2D - Dot2") {
    const FVector2f a(3.0f, 4.0f);
    const FVector2f b(2.0f, -1.0f);
    REQUIRE_CLOSE(Dot2(a, b), 2.0f, 1e-6f);
}

TEST_CASE("Geometry2D - DistPointSegmentSq") {
    const FVector2f a(0.0f, 0.0f);
    const FVector2f b(10.0f, 0.0f);

    REQUIRE_CLOSE(DistPointSegmentSq(FVector2f(5.0f, 3.0f), a, b), 9.0f, 1e-6f);
    REQUIRE_CLOSE(DistPointSegmentSq(FVector2f(12.0f, 0.0f), a, b), 4.0f, 1e-6f);
    REQUIRE_CLOSE(DistPointSegmentSq(FVector2f(0.0f, 0.0f), a, b), 0.0f, 1e-6f);
}
