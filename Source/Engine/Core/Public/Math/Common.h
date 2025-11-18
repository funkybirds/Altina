#pragma once

#include "Types/Aliases.h"
#include "Types/Concepts.h"
#include "../Base/CoreAPI.h"

#include "../Platform/PlatformIntrinsic.h"

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

} // namespace AltinaEngine::Core::Math