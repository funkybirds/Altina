#pragma once

#include "Types/Aliases.h"
#include "Types/Concepts.h"

namespace AltinaEngine::Core::Math
{

    template <CScalar T, u32 ComponentCount> struct TVector
    {
        T                            mComponents[ComponentCount]{};

        [[nodiscard]] constexpr auto operator[](u32 Index) noexcept -> T& { return mComponents[Index]; }
        [[nodiscard]] constexpr auto operator[](u32 Index) const noexcept -> const T& { return mComponents[Index]; }

        // Convenience constructors for common vector sizes
        explicit constexpr TVector(T a, T b) noexcept
            requires(ComponentCount == 2)
        {
            mComponents[0] = a;
            mComponents[1] = b;
        }

        explicit constexpr TVector(T a, T b, T c) noexcept
            requires(ComponentCount == 3)
        {
            mComponents[0] = a;
            mComponents[1] = b;
            mComponents[2] = c;
        }

        explicit constexpr TVector(T a, T b, T c, T d) noexcept
            requires(ComponentCount == 4)
        {
            mComponents[0] = a;
            mComponents[1] = b;
            mComponents[2] = c;
            mComponents[3] = d;
        }

        // Set all components to the same value
        explicit constexpr TVector(T x) noexcept
        {
            for (u32 i = 0U; i < ComponentCount; ++i)
            {
                mComponents[i] = x;
            }
        }

        // Component accessors
        [[nodiscard]] constexpr auto X() noexcept -> T&
            requires(ComponentCount >= 1)
        {
            return mComponents[0];
        }
        [[nodiscard]] constexpr auto X() const noexcept -> const T&
            requires(ComponentCount >= 1)
        {
            return mComponents[0];
        }

        [[nodiscard]] constexpr auto Y() noexcept -> T&
            requires(ComponentCount >= 2)
        {
            return mComponents[1];
        }
        [[nodiscard]] constexpr auto Y() const noexcept -> const T&
            requires(ComponentCount >= 2)
        {
            return mComponents[1];
        }

        [[nodiscard]] constexpr auto Z() noexcept -> T&
            requires(ComponentCount >= 3)
        {
            return mComponents[2];
        }
        [[nodiscard]] constexpr auto Z() const noexcept -> const T&
            requires(ComponentCount >= 3)
        {
            return mComponents[2];
        }

        [[nodiscard]] constexpr auto W() noexcept -> T&
            requires(ComponentCount >= 4)
        {
            return mComponents[3];
        }
        [[nodiscard]] constexpr auto W() const noexcept -> const T&
            requires(ComponentCount >= 4)
        {
            return mComponents[3];
        }

        constexpr auto operator+=(const TVector& Rhs) noexcept -> TVector&
        {
            for (u32 i = 0U; i < ComponentCount; ++i)
            {
                mComponents[i] += Rhs.mComponents[i];
            }
            return *this;
        }

        constexpr auto operator-=(const TVector& Rhs) noexcept -> TVector&
        {
            for (u32 i = 0U; i < ComponentCount; ++i)
            {
                mComponents[i] -= Rhs.mComponents[i];
            }
            return *this;
        }

        constexpr auto operator*=(const TVector& Rhs) noexcept -> TVector&
        {
            for (u32 i = 0U; i < ComponentCount; ++i)
            {
                mComponents[i] *= Rhs.mComponents[i];
            }
            return *this;
        }

        constexpr auto operator/=(const TVector& Rhs) noexcept -> TVector&
        {
            for (u32 i = 0U; i < ComponentCount; ++i)
            {
                mComponents[i] /= Rhs.mComponents[i];
            }
            return *this;
        }
    };

    template <CScalar T, u32 ComponentCount>
    [[nodiscard]] constexpr auto operator+(
        TVector<T, ComponentCount> Lhs, const TVector<T, ComponentCount>& Rhs) noexcept -> TVector<T, ComponentCount>
    {
        Lhs += Rhs;
        return Lhs;
    }

    template <CScalar T, u32 ComponentCount>
    [[nodiscard]] constexpr auto operator-(
        TVector<T, ComponentCount> Lhs, const TVector<T, ComponentCount>& Rhs) noexcept -> TVector<T, ComponentCount>
    {
        Lhs -= Rhs;
        return Lhs;
    }

    template <CScalar T, u32 ComponentCount>
    [[nodiscard]] constexpr auto operator*(
        TVector<T, ComponentCount> Lhs, const TVector<T, ComponentCount>& Rhs) noexcept -> TVector<T, ComponentCount>
    {
        Lhs *= Rhs;
        return Lhs;
    }

    template <CScalar T, u32 ComponentCount>
    [[nodiscard]] constexpr auto operator/(
        TVector<T, ComponentCount> Lhs, const TVector<T, ComponentCount>& Rhs) noexcept -> TVector<T, ComponentCount>
    {
        Lhs /= Rhs;
        return Lhs;
    }

    using FVector2f = TVector<f32, 2U>; // NOLINT(*-identifier-naming)
    using FVector3f = TVector<f32, 3U>; // NOLINT(*-identifier-naming)
    using FVector4f = TVector<f32, 4U>; // NOLINT(*-identifier-naming)

    using FVector2i = TVector<i32, 2U>; // NOLINT(*-identifier-naming)
    using FVector3i = TVector<i32, 3U>; // NOLINT(*-identifier-naming)
    using FVector4i = TVector<i32, 4U>; // NOLINT(*-identifier-naming)

} // namespace AltinaEngine::Core::Math
