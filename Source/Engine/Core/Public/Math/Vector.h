#pragma once

#include "Types/Aliases.h"
#include "Types/Concepts.h"
#include "Base/CoreAPI.h"

namespace AltinaEngine::Core::Math
{

    template <IScalar T, u32 ComponentCount> struct TVector
    {
        T                                Components[ComponentCount]{};

        [[nodiscard]] constexpr T&       operator[](u32 Index) noexcept { return Components[Index]; }
        [[nodiscard]] constexpr const T& operator[](u32 Index) const noexcept { return Components[Index]; }

        // Convenience constructors for common vector sizes
        explicit constexpr TVector(T a, T b) noexcept
            requires(ComponentCount == 2)
        {
            Components[0] = a;
            Components[1] = b;
        }

        explicit constexpr TVector(T a, T b, T c) noexcept
            requires(ComponentCount == 3)
        {
            Components[0] = a;
            Components[1] = b;
            Components[2] = c;
        }

        explicit constexpr TVector(T a, T b, T c, T d) noexcept
            requires(ComponentCount == 4)
        {
            Components[0] = a;
            Components[1] = b;
            Components[2] = c;
            Components[3] = d;
        }

        // Set all components to the same value
        explicit constexpr TVector(T x) noexcept
        {
            for (u32 i = 0U; i < ComponentCount; ++i)
            {
                Components[i] = x;
            }
        }

        // Component accessors
        [[nodiscard]] constexpr T& X() noexcept
            requires(ComponentCount >= 1)
        {
            return Components[0];
        }
        [[nodiscard]] constexpr const T& X() const noexcept
            requires(ComponentCount >= 1)
        {
            return Components[0];
        }

        [[nodiscard]] constexpr T& Y() noexcept
            requires(ComponentCount >= 2)
        {
            return Components[1];
        }
        [[nodiscard]] constexpr const T& Y() const noexcept
            requires(ComponentCount >= 2)
        {
            return Components[1];
        }

        [[nodiscard]] constexpr T& Z() noexcept
            requires(ComponentCount >= 3)
        {
            return Components[2];
        }
        [[nodiscard]] constexpr const T& Z() const noexcept
            requires(ComponentCount >= 3)
        {
            return Components[2];
        }

        [[nodiscard]] constexpr T& W() noexcept
            requires(ComponentCount >= 4)
        {
            return Components[3];
        }
        [[nodiscard]] constexpr const T& W() const noexcept
            requires(ComponentCount >= 4)
        {
            return Components[3];
        }

        constexpr TVector& operator+=(const TVector& Rhs) noexcept
        {
            for (u32 i = 0U; i < ComponentCount; ++i)
            {
                Components[i] += Rhs.Components[i];
            }
            return *this;
        }

        constexpr TVector& operator-=(const TVector& Rhs) noexcept
        {
            for (u32 i = 0U; i < ComponentCount; ++i)
            {
                Components[i] -= Rhs.Components[i];
            }
            return *this;
        }

        constexpr TVector& operator*=(const TVector& Rhs) noexcept
        {
            for (u32 i = 0U; i < ComponentCount; ++i)
            {
                Components[i] *= Rhs.Components[i];
            }
            return *this;
        }

        constexpr TVector& operator/=(const TVector& Rhs) noexcept
        {
            for (u32 i = 0U; i < ComponentCount; ++i)
            {
                Components[i] /= Rhs.Components[i];
            }
            return *this;
        }
    };

    template <IScalar T, u32 ComponentCount>
    [[nodiscard]] constexpr TVector<T, ComponentCount> operator+(
        TVector<T, ComponentCount> Lhs, const TVector<T, ComponentCount>& Rhs) noexcept
    {
        Lhs += Rhs;
        return Lhs;
    }

    template <IScalar T, u32 ComponentCount>
    [[nodiscard]] constexpr TVector<T, ComponentCount> operator-(
        TVector<T, ComponentCount> Lhs, const TVector<T, ComponentCount>& Rhs) noexcept
    {
        Lhs -= Rhs;
        return Lhs;
    }

    template <IScalar T, u32 ComponentCount>
    [[nodiscard]] constexpr TVector<T, ComponentCount> operator*(
        TVector<T, ComponentCount> Lhs, const TVector<T, ComponentCount>& Rhs) noexcept
    {
        Lhs *= Rhs;
        return Lhs;
    }

    template <IScalar T, u32 ComponentCount>
    [[nodiscard]] constexpr TVector<T, ComponentCount> operator/(
        TVector<T, ComponentCount> Lhs, const TVector<T, ComponentCount>& Rhs) noexcept
    {
        Lhs /= Rhs;
        return Lhs;
    }

    using FVector2f = TVector<f32, 2U>;
    using FVector3f = TVector<f32, 3U>;
    using FVector4f = TVector<f32, 4U>;

    using FVector2i = TVector<i32, 2U>;
    using FVector3i = TVector<i32, 3U>;
    using FVector4i = TVector<i32, 4U>;

} // namespace AltinaEngine::Core::Math
