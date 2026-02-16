#pragma once

#include "Math/Common.h"
#include "Math/Matrix.h"

namespace AltinaEngine::Core::Math::LinAlg {

    // Projection matrix in engine default clip-space:
    // Y up, X right, NDC Y+ is up, NDC Z range [0, 1].
    template <CFloatingPoint T> struct TProjectionMatrix : public TMatrix<T, 4U, 4U> {
        using Base = TMatrix<T, 4U, 4U>;
        using Base::Base;

        // Perspective projection using vertical FOV (radians) and view dimensions.
        TProjectionMatrix(T fovYRadians, T viewX, T viewY, T minZ, T maxZ) noexcept : Base(T{}) {
            const T halfFov = fovYRadians * static_cast<T>(0.5);
            const T yScale  = static_cast<T>(1) / Tan(halfFov);
            const T xScale  = yScale * (viewY / viewX);
            const T zRange  = maxZ - minZ;

            (*this)(0, 0) = xScale;
            (*this)(1, 1) = yScale;
            (*this)(2, 2) = maxZ / zRange;
            (*this)(2, 3) = -minZ * maxZ / zRange;
            (*this)(3, 2) = static_cast<T>(1);
        }
    };

    using FProjectionMatrixf = TProjectionMatrix<f32>; // NOLINT(*-identifier-naming)
    using FProjectionMatrixd = TProjectionMatrix<f64>; // NOLINT(*-identifier-naming)

    // Reversed-Z projection matrix (NDC Z range [0, 1], near maps to 1, far maps to 0).
    template <CFloatingPoint T> struct TReversedZProjectionMatrix : public TMatrix<T, 4U, 4U> {
        using Base = TMatrix<T, 4U, 4U>;
        using Base::Base;

        TReversedZProjectionMatrix(T fovYRadians, T viewX, T viewY, T minZ, T maxZ) noexcept
            : Base(T{}) {
            const T halfFov = fovYRadians * static_cast<T>(0.5);
            const T yScale  = static_cast<T>(1) / Tan(halfFov);
            const T xScale  = yScale * (viewY / viewX);
            const T zRange  = minZ - maxZ;

            (*this)(0, 0) = xScale;
            (*this)(1, 1) = yScale;
            (*this)(2, 2) = minZ / zRange;
            (*this)(2, 3) = -minZ * maxZ / zRange;
            (*this)(3, 2) = static_cast<T>(1);
        }
    };

    using FReversedZProjectionMatrixf =
        TReversedZProjectionMatrix<f32>; // NOLINT(*-identifier-naming)
    using FReversedZProjectionMatrixd =
        TReversedZProjectionMatrix<f64>; // NOLINT(*-identifier-naming)

} // namespace AltinaEngine::Core::Math::LinAlg
