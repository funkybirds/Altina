#pragma once

#include "Types/Aliases.h"
#include "Types/Concepts.h"
#include "../Base/CoreAPI.h"

#include "../Platform/Generic/PlatformIntrinsicGeneric.h"

namespace AltinaEngine::Core::Math
{

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

    // Floor / Ceil (signed integral destination, floating-point source)
    template <ISignedIntegral TDst, IFloatingPoint TSrc>
    [[nodiscard]] AE_FORCEINLINE constexpr TDst Floor(TSrc value) noexcept
    {
        const TDst truncated = static_cast<TDst>(value);
        return (static_cast<TSrc>(truncated) > value) ? static_cast<TDst>(truncated - static_cast<TDst>(1))
                                                      : truncated;
    }

    template <ISignedIntegral TDst, IFloatingPoint TSrc>
    [[nodiscard]] AE_FORCEINLINE constexpr TDst Ceil(TSrc value) noexcept
    {
        const TDst truncated = static_cast<TDst>(value);
        return (static_cast<TSrc>(truncated) < value) ? static_cast<TDst>(truncated + static_cast<TDst>(1))
                                                      : truncated;
    }

    // Linear interpolation (floating point only, identical types)
    template <IFloatingPoint T>
    [[nodiscard]] AE_FORCEINLINE constexpr T Lerp(T a, T b, T t) noexcept
    {
        return a + (b - a) * t;
    }

    // Clamp (scalar only, identical types)
    template <IScalar T>
    [[nodiscard]] AE_FORCEINLINE constexpr T Clamp(T value, T minValue, T maxValue) noexcept
    {
        const T clampedLower = (value < minValue) ? minValue : value;
        return (clampedLower > maxValue) ? maxValue : clampedLower;
    }

} // namespace AltinaEngine::Core::Math