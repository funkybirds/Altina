#pragma once

#include "Types/Aliases.h"
#include "Types/Concepts.h"
#include "Vector.h"
#include "Base/CoreAPI.h"

namespace AltinaEngine::Core::Math
{
    template <IScalar T, u32 Rows, u32 Cols> struct TMatrix
    {
        T                                Elements[Rows][Cols]{};

        [[nodiscard]] constexpr T&       operator()(u32 Row, u32 Col) noexcept { return Elements[Row][Col]; }
        [[nodiscard]] constexpr const T& operator()(u32 Row, u32 Col) const noexcept { return Elements[Row][Col]; }

        // Row access: returns pointer to row elements (Cols length)
        [[nodiscard]] constexpr T*       operator[](u32 Row) noexcept { return Elements[Row]; }
        [[nodiscard]] constexpr const T* operator[](u32 Row) const noexcept { return Elements[Row]; }

        constexpr TMatrix() = default;

        // Fill constructor
        explicit constexpr TMatrix(T v) noexcept
        {
            for (u32 r = 0U; r < Rows; ++r)
            {
                for (u32 c = 0U; c < Cols; ++c)
                {
                    Elements[r][c] = v;
                }
            }
        }
    };

    using FMatrix3x3f = TMatrix<f32, 3U, 3U>;
    using FMatrix4x4f = TMatrix<f32, 4U, 4U>;
    // legacy aliases
    using FMatrix3x3 = FMatrix3x3f;
    using FMatrix4x4 = FMatrix4x4f;

} // namespace AltinaEngine::Core::Math

// Element-wise matrix arithmetic
namespace AltinaEngine::Core::Math
{
    template <IScalar T, u32 R, u32 C>
    [[nodiscard]] constexpr TMatrix<T, R, C> operator+(const TMatrix<T, R, C>& A, const TMatrix<T, R, C>& B) noexcept
    {
        TMatrix<T, R, C> Out;
        for (u32 r = 0U; r < R; ++r)
        {
            for (u32 c = 0U; c < C; ++c)
            {
                Out(r, c) = A.Elements[r][c] + B.Elements[r][c];
            }
        }
        return Out;
    }

    template <IScalar T, u32 R, u32 C>
    [[nodiscard]] constexpr TMatrix<T, R, C> operator-(const TMatrix<T, R, C>& A, const TMatrix<T, R, C>& B) noexcept
    {
        TMatrix<T, R, C> Out;
        for (u32 r = 0U; r < R; ++r)
        {
            for (u32 c = 0U; c < C; ++c)
            {
                Out(r, c) = A.Elements[r][c] - B.Elements[r][c];
            }
        }
        return Out;
    }

    template <IScalar T, u32 R, u32 C>
    [[nodiscard]] constexpr TMatrix<T, R, C> operator*(const TMatrix<T, R, C>& A, const TMatrix<T, R, C>& B) noexcept
    {
        TMatrix<T, R, C> Out;
        for (u32 r = 0U; r < R; ++r)
        {
            for (u32 c = 0U; c < C; ++c)
            {
                Out(r, c) = A.Elements[r][c] * B.Elements[r][c];
            }
        }
        return Out;
    }

    template <IScalar T, u32 R, u32 C>
    [[nodiscard]] constexpr TMatrix<T, R, C> operator/(const TMatrix<T, R, C>& A, const TMatrix<T, R, C>& B) noexcept
    {
        TMatrix<T, R, C> Out;
        for (u32 r = 0U; r < R; ++r)
        {
            for (u32 c = 0U; c < C; ++c)
            {
                Out(r, c) = A.Elements[r][c] / B.Elements[r][c];
            }
        }
        return Out;
    }

} // namespace AltinaEngine::Core::Math

// Transpose: (R x C) -> (C x R)
namespace AltinaEngine::Core::Math
{
    template <IScalar T, u32 R, u32 C>
    [[nodiscard]] constexpr TMatrix<T, C, R> Transpose(const TMatrix<T, R, C>& M) noexcept
    {
        TMatrix<T, C, R> Out(T{});
        for (u32 r = 0U; r < R; ++r)
        {
            for (u32 c = 0U; c < C; ++c)
            {
                Out(c, r) = M.Elements[r][c];
            }
        }
        return Out;
    }

} // namespace AltinaEngine::Core::Math

// Matrix * Vector multiplication (MxN) * (N) -> (M)
namespace AltinaEngine::Core::Math
{
    template <IScalar T, u32 R, u32 C>
    [[nodiscard]] constexpr TVector<T, R> MatMul(const TMatrix<T, R, C>& M, const TVector<T, C>& v) noexcept
    {
        TVector<T, R> Out(T{});
        for (u32 r = 0U; r < R; ++r)
        {
            T sum = T{};
            for (u32 c = 0U; c < C; ++c)
            {
                sum += M.Elements[r][c] * v[c];
            }
            Out[r] = sum;
        }
        return Out;
    }

} // namespace AltinaEngine::Core::Math

// Matrix * Matrix multiplication (R x K) * (K x C) -> (R x C)
namespace AltinaEngine::Core::Math
{
    template <IScalar T, u32 R, u32 K, u32 C>
    [[nodiscard]] constexpr TMatrix<T, R, C> MatMul(const TMatrix<T, R, K>& A, const TMatrix<T, K, C>& B) noexcept
    {
        TMatrix<T, R, C> Out(T{});
        for (u32 r = 0U; r < R; ++r)
        {
            for (u32 c = 0U; c < C; ++c)
            {
                T sum = T{};
                for (u32 k = 0U; k < K; ++k)
                {
                    sum += A.Elements[r][k] * B.Elements[k][c];
                }
                Out(r, c) = sum;
            }
        }
        return Out;
    }

} // namespace AltinaEngine::Core::Math
