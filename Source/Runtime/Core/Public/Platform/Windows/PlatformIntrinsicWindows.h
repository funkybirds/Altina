#pragma once

#include "../Generic/GenericPlatformDecl.h"
#include "../Generic/PlatformIntrinsicGeneric.h"

#if AE_PLATFORM_WIN
    #include <intrin.h>

namespace AltinaEngine::Core::Platform {

    constexpr auto PopCount32(u32 Value) noexcept -> u32 {
        return (IsConstantEvaluated()) ? Detail::PopCount32Impl(Value) : __popcnt(Value);
    }

    constexpr auto PopCount64(u64 Value) noexcept -> u32 {
        return (IsConstantEvaluated()) ? Detail::PopCount64Impl(Value)
                                       : static_cast<u32>(__popcnt64(Value));
    }

    constexpr auto CountLeadingZeros32(u32 Value) noexcept -> u32 {
        if (IsConstantEvaluated()) {
            return Detail::CountLeadingZeros32Impl(Value);
        }
        if (Value == 0U) {
            return 32U;
        }
        unsigned long index = 0;
        _BitScanReverse(&index, Value);
        return 31U - static_cast<u32>(index);
    }

    constexpr auto CountLeadingZeros64(u64 Value) noexcept -> u32 {
        if (IsConstantEvaluated()) {
            return Detail::CountLeadingZeros64Impl(Value);
        }
        if (Value == 0ULL) {
            return 64U;
        }
        unsigned long index = 0;
        _BitScanReverse64(&index, Value);
        return 63U - static_cast<u32>(index);
    }

    constexpr auto CountTrailingZeros32(u32 Value) noexcept -> u32 {
        if (IsConstantEvaluated()) {
            return Detail::CountTrailingZeros32Impl(Value);
        }
        if (Value == 0U) {
            return 32U;
        }
        unsigned long index = 0;
        _BitScanForward(&index, Value);
        return static_cast<u32>(index);
    }

    constexpr auto CountTrailingZeros64(u64 Value) noexcept -> u32 {
        if (IsConstantEvaluated()) {
            return Detail::CountTrailingZeros64Impl(Value);
        }
        if (Value == 0ULL) {
            return 64U;
        }
        unsigned long index = 0;
        _BitScanForward64(&index, Value);
        return static_cast<u32>(index);
    }

} // namespace AltinaEngine::Core::Platform

#endif // AE_PLATFORM_WIN