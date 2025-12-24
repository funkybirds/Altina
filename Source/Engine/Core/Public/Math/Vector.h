#pragma once

#include "Types/Aliases.h"
#include "Types/Concepts.h"
#include "Base/CoreAPI.h"

namespace AltinaEngine::Core::Math
{
    
    template <typename T, u32 ComponentCount> struct TVector
    {
        static_assert(AltinaEngine::IScalar<T>, "TVector requires scalar component types");

        T                                Components[ComponentCount]{};

        [[nodiscard]] constexpr T&       operator[](u32 Index) noexcept { return Components[Index]; }
        [[nodiscard]] constexpr const T& operator[](u32 Index) const noexcept { return Components[Index]; }

        constexpr TVector&               operator+=(const TVector& Rhs) noexcept
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

    template <typename T, u32 ComponentCount>
    [[nodiscard]] constexpr TVector<T, ComponentCount> operator+(
        TVector<T, ComponentCount> Lhs, const TVector<T, ComponentCount>& Rhs) noexcept
    {
        Lhs += Rhs;
        return Lhs;
    }

    template <typename T, u32 ComponentCount>
    [[nodiscard]] constexpr TVector<T, ComponentCount> operator-(
        TVector<T, ComponentCount> Lhs, const TVector<T, ComponentCount>& Rhs) noexcept
    {
        Lhs -= Rhs;
        return Lhs;
    }

    template <typename T, u32 ComponentCount>
    [[nodiscard]] constexpr TVector<T, ComponentCount> operator*(
        TVector<T, ComponentCount> Lhs, const TVector<T, ComponentCount>& Rhs) noexcept
    {
        Lhs *= Rhs;
        return Lhs;
    }

    template <typename T, u32 ComponentCount>
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
