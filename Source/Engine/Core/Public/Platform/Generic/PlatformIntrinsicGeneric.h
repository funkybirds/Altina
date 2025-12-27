#pragma once
#include "../Generic/GenericPlatformDecl.h"

namespace AltinaEngine::Core::Platform::Detail
{

    constexpr auto PopCount32Impl(u32 Value) noexcept -> u32
    {
        u32 count = 0;
        while (Value)
        {
            count += Value & 1U;
            Value >>= 1U;
        }
        return count;
    }

    constexpr auto PopCount64Impl(u64 Value) noexcept -> u32
    {
        u32 count = 0;
        while (Value)
        {
            count += static_cast<u32>(Value & 1ULL);
            Value >>= 1ULL;
        }
        return count;
    }

    constexpr auto CountLeadingZeros32Impl(u32 Value) noexcept -> u32
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

    constexpr auto CountLeadingZeros64Impl(u64 Value) noexcept -> u32
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

    constexpr auto CountTrailingZeros32Impl(u32 Value) noexcept -> u32
    {
        if (Value == 0U)
        {
            return 32U;
        }
        u32 count = 0;
        while ((Value & 1U) == 0U)
        {
            ++count;
            Value >>= 1U;
        }
        return count;
    }

    constexpr auto CountTrailingZeros64Impl(u64 Value) noexcept -> u32
    {
        if (Value == 0ULL)
        {
            return 64U;
        }
        u32 count = 0;
        while ((Value & 1ULL) == 0ULL)
        {
            ++count;
            Value >>= 1ULL;
        }
        return count;
    }

} // namespace AltinaEngine::Core::Platform::Detail