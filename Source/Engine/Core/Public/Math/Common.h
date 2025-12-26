#pragma once

#include "Types/Aliases.h"
#include "Types/Concepts.h"
#include "../Base/CoreAPI.h"

#include "../Platform/Generic/PlatformIntrinsicGeneric.h"

namespace AltinaEngine::Core::Math
{
    // Details
    namespace Details
    {
        AE_CORE_API f32 SinF(f32 radians) noexcept;
        AE_CORE_API f64 SinD(f64 radians) noexcept;
        AE_CORE_API f32 CosF(f32 radians) noexcept;
        AE_CORE_API f64 CosD(f64 radians) noexcept;
        AE_CORE_API f32 SqrtF(f32 value) noexcept;
        AE_CORE_API f64 SqrtD(f64 value) noexcept;
    } // namespace Details

    // Math Constants
    inline constexpr f32 kPiF       = 3.14159265358979323846f;
    inline constexpr f64 kPiD       = 3.14159265358979323846;
    inline constexpr f32 kTwoPiF    = 6.28318530717958647692f;
    inline constexpr f64 kTwoPiD    = 6.28318530717958647692;
    inline constexpr f32 kHalfPiF   = 1.57079632679489661923f;
    inline constexpr f64 kHalfPiD   = 1.57079632679489661923;
    inline constexpr f32 kInvPiF    = 0.31830988618379067154f;
    inline constexpr f64 kInvPiD    = 0.31830988618379067154;
    inline constexpr f32 kInvTwoPiF = 0.15915494309189533577f;
    inline constexpr f64 kInvTwoPiD = 0.15915494309189533577;

    // Casting Utilities
    template <IIntegral TDst, IFloatingPoint TSrc>
    [[nodiscard]] AE_FORCEINLINE constexpr TDst TruncatedCast(TSrc Value) noexcept
    {
        return static_cast<TDst>(Value);
    }

    template <IIntegral TDst, IFloatingPoint TSrc>
    [[nodiscard]] AE_FORCEINLINE constexpr TDst RoundedCast(TSrc Value) noexcept
    {
        return static_cast<TDst>(
            Value + (Value >= static_cast<TSrc>(0) ? static_cast<TSrc>(0.5) : static_cast<TSrc>(-0.5)));
    }

    // Generic Utilities
    template <IIntegral T> [[nodiscard]] AE_FORCEINLINE constexpr T DivRoundUp(T Numerator, T Denominator) noexcept
    {
        return (Numerator + Denominator - static_cast<T>(1)) / Denominator;
    }

    template <IIntegral T> [[nodiscard]] AE_FORCEINLINE constexpr T IntegerLog2(T value) noexcept
    {
        T result = static_cast<T>(0);
        while (value >>= static_cast<T>(1))
        {
            ++result;
        }
        return result;
    }

    template <> [[nodiscard]] AE_FORCEINLINE constexpr u32 IntegerLog2<u32>(u32 value) noexcept
    {
        return 31U - Platform::CountLeadingZeros32(value);
    }
    template <> [[nodiscard]] AE_FORCEINLINE constexpr u64 IntegerLog2<u64>(u64 value) noexcept
    {
        return 63U - Platform::CountLeadingZeros64(value);
    }

    // Max / Min (scalar only, identical types)
    template <IScalar T, IScalar... Ts>
        requires(ISameAsAll<T, Ts...>)
    [[nodiscard]] AE_FORCEINLINE constexpr T Max(T first, Ts... rest) noexcept
    {
        if constexpr (sizeof...(rest) == 0)
        {
            return first;
        }

        T result = first;
        ((result = (rest > result) ? rest : result), ...);
        return result;
    }

    template <IScalar T, IScalar... Ts>
        requires(ISameAsAll<T, Ts...>)
    [[nodiscard]] AE_FORCEINLINE constexpr T Min(T first, Ts... rest) noexcept
    {
        if constexpr (sizeof...(rest) == 0)
        {
            return first;
        }

        T result = first;
        ((result = (rest < result) ? rest : result), ...);
        return result;
    }

    // Floor / Ceil (signed integral destination, f32ing-point source)
    template <ISignedIntegral TDst, IFloatingPoint TSrc>
    [[nodiscard]] AE_FORCEINLINE constexpr TDst Floor(TSrc value) noexcept
    {
        const TDst truncated = static_cast<TDst>(value);
        return (static_cast<TSrc>(truncated) > value) ? static_cast<TDst>(truncated - static_cast<TDst>(1)) : truncated;
    }

    template <ISignedIntegral TDst, IFloatingPoint TSrc>
    [[nodiscard]] AE_FORCEINLINE constexpr TDst Ceil(TSrc value) noexcept
    {
        const TDst truncated = static_cast<TDst>(value);
        return (static_cast<TSrc>(truncated) < value) ? static_cast<TDst>(truncated + static_cast<TDst>(1)) : truncated;
    }

    // Linear interpolation (f32ing point only, identical types)
    template <IFloatingPoint T> [[nodiscard]] AE_FORCEINLINE constexpr T Lerp(T a, T b, T t) noexcept
    {
        return a + (b - a) * t;
    }

    // Clamp (scalar only, identical types)
    template <IScalar T> [[nodiscard]] AE_FORCEINLINE constexpr T Clamp(T value, T minValue, T maxValue) noexcept
    {
        const T clampedLower = (value < minValue) ? minValue : value;
        return (clampedLower > maxValue) ? maxValue : clampedLower;
    }

    // Abs
    template <ISignedIntegral T> [[nodiscard]] AE_FORCEINLINE constexpr T Abs(T value) noexcept
    {
        return (value < static_cast<T>(0)) ? -value : value;
    }
    template <IFloatingPoint T> [[nodiscard]] AE_FORCEINLINE constexpr T Abs(T value) noexcept
    {
        return (value < static_cast<T>(0)) ? -value : value;
    }

    // Sin, Cos (radians, floating-point)
    template <IFloatingPoint T> [[nodiscard]] AE_FORCEINLINE T Sin(T radians) noexcept
    {
        if constexpr (AltinaEngine::TTypeSameAs_v<T, f32>)
        {
            return Details::SinF(radians);
        }
        else
        {
            return static_cast<T>(Details::SinD(static_cast<f64>(radians)));
        }
    }

    template <IFloatingPoint T> [[nodiscard]] AE_FORCEINLINE T Cos(T radians) noexcept
    {
        if constexpr (AltinaEngine::TTypeSameAs_v<T, f32>)
        {
            return Details::CosF(radians);
        }
        else
        {
            return static_cast<T>(Details::CosD(static_cast<f64>(radians)));
        }
    }

    template <IFloatingPoint T> [[nodiscard]] AE_FORCEINLINE T Tan(T radians) noexcept
    {
        return Sin(radians) / Cos(radians);
    }

    // Sqrt (floating-point)
    template <IFloatingPoint T> [[nodiscard]] AE_FORCEINLINE T Sqrt(T value) noexcept
    {
        if constexpr (AltinaEngine::TTypeSameAs_v<T, f32>)
        {
            return Details::SqrtF(value);
        }
        else
        {
            return static_cast<T>(Details::SqrtD(static_cast<f64>(value)));
        }
    }

} // namespace AltinaEngine::Core::Math