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
        AE_CORE_API auto SinF(f32 radians) noexcept -> f32;
        AE_CORE_API auto SinD(f64 radians) noexcept -> f64;
        AE_CORE_API auto CosF(f32 radians) noexcept -> f32;
        AE_CORE_API auto CosD(f64 radians) noexcept -> f64;
        AE_CORE_API auto SqrtF(f32 value) noexcept -> f32;
        AE_CORE_API auto SqrtD(f64 value) noexcept -> f64;
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
    [[nodiscard]] AE_FORCEINLINE constexpr auto TruncatedCast(TSrc Value) noexcept -> TDst
    {
        return static_cast<TDst>(Value);
    }

    template <IIntegral TDst, IFloatingPoint TSrc>
    [[nodiscard]] AE_FORCEINLINE constexpr auto RoundedCast(TSrc Value) noexcept -> TDst
    {
        return static_cast<TDst>(
            Value + (Value >= static_cast<TSrc>(0) ? static_cast<TSrc>(0.5) : static_cast<TSrc>(-0.5)));
    }

    // Generic Utilities
    template <IIntegral T>
    [[nodiscard]] AE_FORCEINLINE constexpr auto DivRoundUp(T Numerator, T Denominator) noexcept -> T
    {
        return (Numerator + Denominator - static_cast<T>(1)) / Denominator;
    }

    template <IIntegral T> [[nodiscard]] AE_FORCEINLINE constexpr auto IntegerLog2(T value) noexcept -> T
    {
        T result = static_cast<T>(0);
        while (value >>= static_cast<T>(1))
        {
            ++result;
        }
        return result;
    }

    template <> [[nodiscard]] AE_FORCEINLINE constexpr auto IntegerLog2<u32>(u32 value) noexcept -> u32
    {
        return 31U - Platform::CountLeadingZeros32(value);
    }
    template <> [[nodiscard]] AE_FORCEINLINE constexpr auto IntegerLog2<u64>(u64 value) noexcept -> u64
    {
        return 63U - Platform::CountLeadingZeros64(value);
    }

    // Max / Min (scalar only, identical types)
    template <IScalar T, IScalar... Ts>
        requires(ISameAsAll<T, Ts...>)
    [[nodiscard]] AE_FORCEINLINE constexpr auto Max(T first, Ts... rest) noexcept -> T
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
    [[nodiscard]] AE_FORCEINLINE constexpr auto Min(T first, Ts... rest) noexcept -> T
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
    [[nodiscard]] AE_FORCEINLINE constexpr auto Floor(TSrc value) noexcept -> TDst
    {
        const TDst truncated = static_cast<TDst>(value);
        return (static_cast<TSrc>(truncated) > value) ? static_cast<TDst>(truncated - static_cast<TDst>(1)) : truncated;
    }

    template <ISignedIntegral TDst, IFloatingPoint TSrc>
    [[nodiscard]] AE_FORCEINLINE constexpr auto Ceil(TSrc value) noexcept -> TDst
    {
        const TDst truncated = static_cast<TDst>(value);
        return (static_cast<TSrc>(truncated) < value) ? static_cast<TDst>(truncated + static_cast<TDst>(1)) : truncated;
    }

    // Linear interpolation (f32ing point only, identical types)
    template <IFloatingPoint T> [[nodiscard]] AE_FORCEINLINE constexpr auto Lerp(T a, T b, T t) noexcept -> T
    {
        return a + (b - a) * t;
    }

    // Clamp (scalar only, identical types)
    template <IScalar T>
    [[nodiscard]] AE_FORCEINLINE constexpr auto Clamp(T value, T minValue, T maxValue) noexcept -> T
    {
        const T clampedLower = (value < minValue) ? minValue : value;
        return (clampedLower > maxValue) ? maxValue : clampedLower;
    }

    // Abs
    template <ISignedIntegral T> [[nodiscard]] AE_FORCEINLINE constexpr auto Abs(T value) noexcept -> T
    {
        return (value < static_cast<T>(0)) ? -value : value;
    }
    template <IFloatingPoint T> [[nodiscard]] AE_FORCEINLINE constexpr auto Abs(T value) noexcept -> T
    {
        return (value < static_cast<T>(0)) ? -value : value;
    }

    // Sin, Cos (radians, floating-point)
    template <IFloatingPoint T> [[nodiscard]] AE_FORCEINLINE auto Sin(T radians) noexcept -> T
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

    template <IFloatingPoint T> [[nodiscard]] AE_FORCEINLINE auto Cos(T radians) noexcept -> T
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

    template <IFloatingPoint T> [[nodiscard]] AE_FORCEINLINE auto Tan(T radians) noexcept -> T
    {
        return Sin(radians) / Cos(radians);
    }

    // Sqrt (floating-point)
    template <IFloatingPoint T> [[nodiscard]] AE_FORCEINLINE auto Sqrt(T value) noexcept -> T
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