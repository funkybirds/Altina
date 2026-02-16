#pragma once

#include "Math/Matrix.h"

namespace AltinaEngine::Core::Math::LinAlg {

    // Projection matrix in engine default clip-space:
    // Y up, X right, NDC Y+ is up, NDC Z range [0, 1].
    template <CScalar T> struct TProjectionMatrix : public TMatrix<T, 4U, 4U> {
        using Base = TMatrix<T, 4U, 4U>;
        using Base::Base;

        constexpr TProjectionMatrix() noexcept : Base(T{}) {
            for (u32 i = 0U; i < 4U; ++i) {
                (*this)(i, i) = static_cast<T>(1);
            }
        }

        static constexpr auto Identity() noexcept -> TProjectionMatrix {
            return TProjectionMatrix();
        }
    };

    using FProjectionMatrixf = TProjectionMatrix<f32>; // NOLINT(*-identifier-naming)
    using FProjectionMatrixd = TProjectionMatrix<f64>; // NOLINT(*-identifier-naming)

} // namespace AltinaEngine::Core::Math::LinAlg
