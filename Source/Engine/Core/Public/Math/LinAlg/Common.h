#pragma once

#include "Types/Aliases.h"
#include "Types/Concepts.h"
#include "Math/Matrix.h"
#include "Math/Vector.h"
#include "Base/CoreAPI.h"

namespace AltinaEngine::Core::Math::LinAlg
{
    // Identity matrix of size N x N (1 on diagonal, 0 elsewhere)
    template <IScalar T, u32 N> [[nodiscard]] constexpr TMatrix<T, N, N> Identity() noexcept
    {
        TMatrix<T, N, N> M(T{});
        for (u32 i = 0U; i < N; ++i)
        {
            M(i, i) = static_cast<T>(1);
        }
        return M;
    }

    // Matrix trace (sum of diagonal elements)
    template <IScalar T, u32 N> [[nodiscard]] constexpr T MatTrace(const TMatrix<T, N, N>& M) noexcept
    {
        T sum = T{};
        for (u32 i = 0U; i < N; ++i)
        {
            sum += M(i, i);
        }
        return sum;
    }

    // Zero matrix R x C
    template <IScalar T, u32 R, u32 C> [[nodiscard]] constexpr TMatrix<T, R, C> ZeroMatrix() noexcept
    {
        return TMatrix<T, R, C>(T{});
    }

    // Determinant for 2x2
    template <IScalar T> [[nodiscard]] constexpr T Determinant(const TMatrix<T, 2, 2>& M) noexcept
    {
        return M(0, 0) * M(1, 1) - M(0, 1) * M(1, 0);
    }

    // Determinant for 3x3
    template <IScalar T> [[nodiscard]] constexpr T Determinant(const TMatrix<T, 3, 3>& M) noexcept
    {
        return M(0, 0) * (M(1, 1) * M(2, 2) - M(1, 2) * M(2, 1)) - M(0, 1) * (M(1, 0) * M(2, 2) - M(1, 2) * M(2, 0))
            + M(0, 2) * (M(1, 0) * M(2, 1) - M(1, 1) * M(2, 0));
    }

    // Determinant for 4x4 (expand along row 0 using 3x3 minors)
    template <IScalar T> [[nodiscard]] constexpr T Determinant(const TMatrix<T, 4, 4>& M) noexcept
    {
        auto det3_excluding = [&](u32 excludeCol) constexpr -> T {
            // collect columns != excludeCol for rows 1..3
            u32 cols[3];
            for (u32 j = 0U, idx = 0U; j < 4U; ++j)
                if (j != excludeCol)
                    cols[idx++] = j;

            // compute 3x3 determinant
            T a00 = M(1, cols[0]);
            T a01 = M(1, cols[1]);
            T a02 = M(1, cols[2]);

            T a10 = M(2, cols[0]);
            T a11 = M(2, cols[1]);
            T a12 = M(2, cols[2]);

            T a20 = M(3, cols[0]);
            T a21 = M(3, cols[1]);
            T a22 = M(3, cols[2]);

            return a00 * (a11 * a22 - a12 * a21) - a01 * (a10 * a22 - a12 * a20) + a02 * (a10 * a21 - a11 * a20);
        };

        return M(0, 0) * det3_excluding(0) - M(0, 1) * det3_excluding(1) + M(0, 2) * det3_excluding(2)
            - M(0, 3) * det3_excluding(3);
    }

    // Inverse for 2x2
    template <IScalar T> [[nodiscard]] constexpr TMatrix<T, 2, 2> Inverse(const TMatrix<T, 2, 2>& M) noexcept
    {
        T                det    = Determinant(M);
        T                invdet = static_cast<T>(1) / det;
        TMatrix<T, 2, 2> Out;
        Out(0, 0) = M(1, 1) * invdet;
        Out(0, 1) = -M(0, 1) * invdet;
        Out(1, 0) = -M(1, 0) * invdet;
        Out(1, 1) = M(0, 0) * invdet;
        return Out;
    }

    // Inverse for 3x3
    template <IScalar T> [[nodiscard]] constexpr TMatrix<T, 3, 3> Inverse(const TMatrix<T, 3, 3>& M) noexcept
    {
        T                det    = Determinant(M);
        T                invdet = static_cast<T>(1) / det;
        TMatrix<T, 3, 3> Out;

        Out(0, 0) = (M(1, 1) * M(2, 2) - M(1, 2) * M(2, 1)) * invdet;
        Out(0, 1) = -(M(0, 1) * M(2, 2) - M(0, 2) * M(2, 1)) * invdet;
        Out(0, 2) = (M(0, 1) * M(1, 2) - M(0, 2) * M(1, 1)) * invdet;

        Out(1, 0) = -(M(1, 0) * M(2, 2) - M(1, 2) * M(2, 0)) * invdet;
        Out(1, 1) = (M(0, 0) * M(2, 2) - M(0, 2) * M(2, 0)) * invdet;
        Out(1, 2) = -(M(0, 0) * M(1, 2) - M(0, 2) * M(1, 0)) * invdet;

        Out(2, 0) = (M(1, 0) * M(2, 1) - M(1, 1) * M(2, 0)) * invdet;
        Out(2, 1) = -(M(0, 0) * M(2, 1) - M(0, 1) * M(2, 0)) * invdet;
        Out(2, 2) = (M(0, 0) * M(1, 1) - M(0, 1) * M(1, 0)) * invdet;

        return Out;
    }

    // Inverse for 4x4 (templated, works for float/double)
    template <IScalar T> [[nodiscard]] constexpr TMatrix<T, 4, 4> Inverse(const TMatrix<T, 4, 4>& p) noexcept
    {
        // Adapted from common 4x4 inversion algorithm
        T a2323 = p[2][2] * p[3][3] - p[2][3] * p[3][2];
        T a1323 = p[2][1] * p[3][3] - p[2][3] * p[3][1];
        T a1223 = p[2][1] * p[3][2] - p[2][2] * p[3][1];
        T a0323 = p[2][0] * p[3][3] - p[2][3] * p[3][0];
        T a0223 = p[2][0] * p[3][2] - p[2][2] * p[3][0];
        T a0123 = p[2][0] * p[3][1] - p[2][1] * p[3][0];
        T a2313 = p[1][2] * p[3][3] - p[1][3] * p[3][2];
        T a1313 = p[1][1] * p[3][3] - p[1][3] * p[3][1];
        T a1213 = p[1][1] * p[3][2] - p[1][2] * p[3][1];
        T a2312 = p[1][2] * p[2][3] - p[1][3] * p[2][2];
        T a1312 = p[1][1] * p[2][3] - p[1][3] * p[2][1];
        T a1212 = p[1][1] * p[2][2] - p[1][2] * p[2][1];
        T a0313 = p[1][0] * p[3][3] - p[1][3] * p[3][0];
        T a0213 = p[1][0] * p[3][2] - p[1][2] * p[3][0];
        T a0312 = p[1][0] * p[2][3] - p[1][3] * p[2][0];
        T a0212 = p[1][0] * p[2][2] - p[1][2] * p[2][0];
        T a0113 = p[1][0] * p[3][1] - p[1][1] * p[3][0];
        T a0112 = p[1][0] * p[2][1] - p[1][1] * p[2][0];
        T det   = p[0][0] * (p[1][1] * a2323 - p[1][2] * a1323 + p[1][3] * a1223)
            - p[0][1] * (p[1][0] * a2323 - p[1][2] * a0323 + p[1][3] * a0223)
            + p[0][2] * (p[1][0] * a1323 - p[1][1] * a0323 + p[1][3] * a0123)
            - p[0][3] * (p[1][0] * a1223 - p[1][1] * a0223 + p[1][2] * a0123);
        T                invdet = static_cast<T>(1) / det;
        TMatrix<T, 4, 4> inv;
        inv[0][0] = invdet * (p[1][1] * a2323 - p[1][2] * a1323 + p[1][3] * a1223);
        inv[0][1] = -invdet * (p[0][1] * a2323 - p[0][2] * a1323 + p[0][3] * a1223);
        inv[0][2] = invdet * (p[0][1] * a2313 - p[0][2] * a1313 + p[0][3] * a1213);
        inv[0][3] = -invdet * (p[0][1] * a2312 - p[0][2] * a1312 + p[0][3] * a1212);
        inv[1][0] = -invdet * (p[1][0] * a2323 - p[1][2] * a0323 + p[1][3] * a0223);
        inv[1][1] = invdet * (p[0][0] * a2323 - p[0][2] * a0323 + p[0][3] * a0223);
        inv[1][2] = -invdet * (p[0][0] * a2313 - p[0][2] * a0313 + p[0][3] * a0213);
        inv[1][3] = invdet * (p[0][0] * a2312 - p[0][2] * a0312 + p[0][3] * a0212);
        inv[2][0] = invdet * (p[1][0] * a1323 - p[1][1] * a0323 + p[1][3] * a0123);
        inv[2][1] = -invdet * (p[0][0] * a1323 - p[0][1] * a0323 + p[0][3] * a0123);
        inv[2][2] = invdet * (p[0][0] * a1313 - p[0][1] * a0313 + p[0][3] * a0113);
        inv[2][3] = -invdet * (p[0][0] * a1312 - p[0][1] * a0312 + p[0][3] * a0112);
        inv[3][0] = -invdet * (p[1][0] * a1223 - p[1][1] * a0223 + p[1][2] * a0123);
        inv[3][1] = invdet * (p[0][0] * a1223 - p[0][1] * a0223 + p[0][2] * a0123);
        inv[3][2] = -invdet * (p[0][0] * a1213 - p[0][1] * a0213 + p[0][2] * a0113);
        inv[3][3] = invdet * (p[0][0] * a1212 - p[0][1] * a0212 + p[0][2] * a0112);
        return inv;
    }

    // Remap a 3D axis-aligned box from source to destination in homogeneous-space matrix form.
    // Produces a 4x4 affine transform that maps srcMin..srcMax to dstMin..dstMax.
    [[nodiscard]] AE_FORCEINLINE FMatrix4x4f CubeSpaceRemap(
        const FVector3f& srcMin, const FVector3f& srcMax, const FVector3f& dstMin, const FVector3f& dstMax)
    {
        f32         scaleX  = (dstMax.X() - dstMin.X()) / (srcMax.X() - srcMin.X());
        f32         scaleY  = (dstMax.Y() - dstMin.Y()) / (srcMax.Y() - srcMin.Y());
        f32         scaleZ  = (dstMax.Z() - dstMin.Z()) / (srcMax.Z() - srcMin.Z());
        f32         offsetX = dstMin.X() - srcMin.X() * scaleX;
        f32         offsetY = dstMin.Y() - srcMin.Y() * scaleY;
        f32         offsetZ = dstMin.Z() - srcMin.Z() * scaleZ;
        FMatrix4x4f result  = Identity<f32, 4>();
        result[0][0]        = scaleX;
        result[1][1]        = scaleY;
        result[2][2]        = scaleZ;
        result[0][3]        = offsetX;
        result[1][3]        = offsetY;
        result[2][3]        = offsetZ;
        return result;
    }

    // Create a skew-symmetric matrix such that CrossProductMatrix(a) * b = a x b
    [[nodiscard]] AE_FORCEINLINE FMatrix3x3f CrossProductMatrix(const FVector3f& v)
    {
        FMatrix3x3f result = ZeroMatrix<f32, 3, 3>();

        result[0][0] = 0.0f;
        result[0][1] = -v.Z();
        result[0][2] = v.Y();

        result[1][0] = v.Z();
        result[1][1] = 0.0f;
        result[1][2] = -v.X();

        result[2][0] = -v.Y();
        result[2][1] = v.X();
        result[2][2] = 0.0f;

        return result;
    }

} // namespace AltinaEngine::Core::Math::LinAlg
