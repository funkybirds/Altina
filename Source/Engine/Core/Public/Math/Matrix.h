#pragma once

#include "Types/Aliases.h"
#include "Types/Concepts.h"
#include "Vector.h"

namespace AltinaEngine::Core::Math
{
    template <CScalar T, u32 Rows, u32 Cols> struct TMatrix
    {
        T                            mElements[Rows][Cols]{};

        [[nodiscard]] constexpr auto operator()(u32 Row, u32 Col) noexcept -> T& { return mElements[Row][Col]; }
        [[nodiscard]] constexpr auto operator()(u32 Row, u32 Col) const noexcept -> const T&
        {
            return mElements[Row][Col];
        }

        // Row access: returns pointer to row elements (Cols length)
        [[nodiscard]] constexpr auto operator[](u32 Row) noexcept -> T* { return mElements[Row]; }
        [[nodiscard]] constexpr auto operator[](u32 Row) const noexcept -> const T* { return mElements[Row]; }

        constexpr TMatrix() = default;

        // Fill constructor
        explicit constexpr TMatrix(T v) noexcept
        {
            for (u32 r = 0U; r < Rows; ++r)
            {
                for (u32 c = 0U; c < Cols; ++c)
                {
                    mElements[r][c] = v;
                }
            }
        }
    };

    using FMatrix3x3f = TMatrix<f32, 3U, 3U>; // NOLINT(*-identifier-naming)
    using FMatrix4x4f = TMatrix<f32, 4U, 4U>; // NOLINT(*-identifier-naming)

} // namespace AltinaEngine::Core::Math

// Element-wise matrix arithmetic
namespace AltinaEngine::Core::Math
{
    template <CScalar T, u32 R, u32 C>
    [[nodiscard]] constexpr auto operator+(const TMatrix<T, R, C>& A, const TMatrix<T, R, C>& B) noexcept
        -> TMatrix<T, R, C>
    {
        TMatrix<T, R, C> out;
        for (u32 r = 0U; r < R; ++r)
        {
            for (u32 c = 0U; c < C; ++c)
            {
                out(r, c) = A.mElements[r][c] + B.mElements[r][c];
            }
        }
        return out;
    }

    template <CScalar T, u32 R, u32 C>
    [[nodiscard]] constexpr auto operator-(const TMatrix<T, R, C>& A, const TMatrix<T, R, C>& B) noexcept
        -> TMatrix<T, R, C>
    {
        TMatrix<T, R, C> out;
        for (u32 r = 0U; r < R; ++r)
        {
            for (u32 c = 0U; c < C; ++c)
            {
                out(r, c) = A.mElements[r][c] - B.mElements[r][c];
            }
        }
        return out;
    }

    template <CScalar T, u32 R, u32 C>
    [[nodiscard]] constexpr auto operator*(const TMatrix<T, R, C>& A, const TMatrix<T, R, C>& B) noexcept
        -> TMatrix<T, R, C>
    {
        TMatrix<T, R, C> out;
        for (u32 r = 0U; r < R; ++r)
        {
            for (u32 c = 0U; c < C; ++c)
            {
                out(r, c) = A.mElements[r][c] * B.mElements[r][c];
            }
        }
        return out;
    }

    template <CScalar T, u32 R, u32 C>
    [[nodiscard]] constexpr auto operator/(const TMatrix<T, R, C>& A, const TMatrix<T, R, C>& B) noexcept
        -> TMatrix<T, R, C>
    {
        TMatrix<T, R, C> out;
        for (u32 r = 0U; r < R; ++r)
        {
            for (u32 c = 0U; c < C; ++c)
            {
                out(r, c) = A.mElements[r][c] / B.mElements[r][c];
            }
        }
        return out;
    }

} // namespace AltinaEngine::Core::Math

// Transpose: (R x C) -> (C x R)
namespace AltinaEngine::Core::Math
{
    template <CScalar T, u32 R, u32 C>
    [[nodiscard]] constexpr auto Transpose(const TMatrix<T, R, C>& M) noexcept -> TMatrix<T, C, R>
    {
        TMatrix<T, C, R> out(T{});
        for (u32 r = 0U; r < R; ++r)
        {
            for (u32 c = 0U; c < C; ++c)
            {
                out(c, r) = M.mElements[r][c];
            }
        }
        return out;
    }

} // namespace AltinaEngine::Core::Math

// Matrix * Vector multiplication (MxN) * (N) -> (M)
namespace AltinaEngine::Core::Math
{
    template <CScalar T, u32 R, u32 C>
    [[nodiscard]] constexpr auto MatMul(const TMatrix<T, R, C>& M, const TVector<T, C>& v) noexcept -> TVector<T, R>
    {
        TVector<T, R> out(T{});
        for (u32 r = 0U; r < R; ++r)
        {
            T sum = T{};
            for (u32 c = 0U; c < C; ++c)
            {
                sum += M.mElements[r][c] * v[c];
            }
            out[r] = sum;
        }
        return out;
    }

} // namespace AltinaEngine::Core::Math

// Matrix * Matrix multiplication (R x K) * (K x C) -> (R x C)
namespace AltinaEngine::Core::Math
{
    template <CScalar T, u32 R, u32 K, u32 C>
    [[nodiscard]] constexpr auto MatMul(const TMatrix<T, R, K>& A, const TMatrix<T, K, C>& B) noexcept
        -> TMatrix<T, R, C>
    {
        TMatrix<T, R, C> out(T{});
        for (u32 r = 0U; r < R; ++r)
        {
            for (u32 c = 0U; c < C; ++c)
            {
                T sum = T{};
                for (u32 k = 0U; k < K; ++k)
                {
                    sum += A.mElements[r][k] * B.mElements[k][c];
                }
                out(r, c) = sum;
            }
        }
        return out;
    }

} // namespace AltinaEngine::Core::Math
