#pragma once
#include "../Generic/GenericPlatformDecl.h"

namespace AltinaEngine::Core::Platform::Detail
{

    inline constexpr u32 PopCount32Impl(u32 Value) noexcept
    {
        u32 Count = 0;
        while (Value)
        {
            Count += Value & 1U;
            Value >>= 1U;
        }
        return Count;
    }

    inline constexpr u32 PopCount64Impl(u64 Value) noexcept
    {
        u32 Count = 0;
        while (Value)
        {
            Count += static_cast<u32>(Value & 1ULL);
            Value >>= 1ULL;
        }
        return Count;
    }

    inline constexpr u32 CountLeadingZeros32Impl(u32 Value) noexcept
    {
        if (Value == 0U)
        {
            return 32U;
        }
        u32 Count = 0;
        while ((Value & 0x80000000U) == 0U)
        {
            ++Count;
            Value <<= 1U;
        }
        return Count;
    }

    inline constexpr u32 CountLeadingZeros64Impl(u64 Value) noexcept
    {
        if (Value == 0ULL)
        {
            return 64U;
        }
        u32 Count = 0;
        while ((Value & 0x8000000000000000ULL) == 0ULL)
        {
            ++Count;
            Value <<= 1ULL;
        }
        return Count;
    }

    inline constexpr u32 CountTrailingZeros32Impl(u32 Value) noexcept
    {
        if (Value == 0U)
        {
            return 32U;
        }
        u32 Count = 0;
        while ((Value & 1U) == 0U)
        {
            ++Count;
            Value >>= 1U;
        }
        return Count;
    }

    inline constexpr u32 CountTrailingZeros64Impl(u64 Value) noexcept
    {
        if (Value == 0ULL)
        {
            return 64U;
        }
        u32 Count = 0;
        while ((Value & 1ULL) == 0ULL)
        {
            ++Count;
            Value >>= 1ULL;
        }
        return Count;
    }

} // namespace AltinaEngine::Core::Platform::Detail