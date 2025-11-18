#pragma once

#include "../Generic/GenericPlatformDecl.h"
#include "../Generic/PlatformIntrinsicGeneric.h"

#if AE_PLATFORM_WIN
    #include <intrin.h>

namespace AltinaEngine::Core::Platform
{
    inline constexpr u32 PopCount32(u32 Value) noexcept
    {
        return (IsConstantEvaluated()) ? Detail::PopCount32Impl(Value)
                                       : static_cast<u32>(__popcnt(static_cast<unsigned int>(Value)));
    }

    inline constexpr u32 PopCount64(u64 Value) noexcept
    {
        return (IsConstantEvaluated()) ? Detail::PopCount64Impl(Value)
                                       : static_cast<u32>(__popcnt64(static_cast<unsigned long long>(Value)));
    }

    inline constexpr u32 CountLeadingZeros32(u32 Value) noexcept
    {
        if (IsConstantEvaluated())
        {
            return Detail::CountLeadingZeros32Impl(Value);
        }
        if (Value == 0U)
        {
            return 32U;
        }
        unsigned long Index = 0;
        _BitScanReverse(&Index, static_cast<unsigned long>(Value));
        return 31U - static_cast<u32>(Index);
    }

    inline constexpr u32 CountLeadingZeros64(u64 Value) noexcept
    {
        if (IsConstantEvaluated())
        {
            return Detail::CountLeadingZeros64Impl(Value);
        }
        if (Value == 0ULL)
        {
            return 64U;
        }
        unsigned long Index = 0;
        _BitScanReverse64(&Index, static_cast<unsigned long long>(Value));
        return 63U - static_cast<u32>(Index);
    }

    inline constexpr u32 CountTrailingZeros32(u32 Value) noexcept
    {
        if (IsConstantEvaluated())
        {
            return Detail::CountTrailingZeros32Impl(Value);
        }
        if (Value == 0U)
        {
            return 32U;
        }
        unsigned long Index = 0;
        _BitScanForward(&Index, static_cast<unsigned long>(Value));
        return static_cast<u32>(Index);
    }

    inline constexpr u32 CountTrailingZeros64(u64 Value) noexcept
    {
        if (IsConstantEvaluated())
        {
            return Detail::CountTrailingZeros64Impl(Value);
        }
        if (Value == 0ULL)
        {
            return 64U;
        }
        unsigned long Index = 0;
        _BitScanForward64(&Index, static_cast<unsigned long long>(Value));
        return static_cast<u32>(Index);
    }

} // namespace AltinaEngine::Core::Platform

#endif // AE_PLATFORM_WIN