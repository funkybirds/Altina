#include "TestHarness.h"

#include "Math/LinAlg/ProjectionMatrix.h"
#include "Math/Matrix.h"

#include <type_traits>

using namespace AltinaEngine::Core::Math::LinAlg;
using AltinaEngine::f32;
using AltinaEngine::f64;
using AltinaEngine::u32;

static_assert(std::is_base_of_v<AltinaEngine::Core::Math::TMatrix<f32, 4U, 4U>,
    FProjectionMatrixf>);
static_assert(std::is_base_of_v<AltinaEngine::Core::Math::TMatrix<f64, 4U, 4U>,
    FProjectionMatrixd>);

namespace {

    template <typename TMatrixType, typename T>
    void RequireIdentity(const TMatrixType& matrix, T eps) {
        for (u32 r = 0U; r < 4U; ++r) {
            for (u32 c = 0U; c < 4U; ++c) {
                const T expected = (r == c) ? static_cast<T>(1) : static_cast<T>(0);
                REQUIRE_CLOSE(matrix(r, c), expected, eps);
            }
        }
    }

} // namespace

TEST_CASE("ProjectionMatrix - default identity f32") {
    const FProjectionMatrixf matrix;
    RequireIdentity(matrix, 1e-6f);
}

TEST_CASE("ProjectionMatrix - default identity f64") {
    const FProjectionMatrixd matrix;
    RequireIdentity(matrix, 1e-12);
}
