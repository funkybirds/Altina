#pragma once

#include "../../Types/Aliases.h"
#include "../../Types/Concepts.h"
#include "../../Base/CoreAPI.h"

namespace AltinaEngine::Core::Platform
{

    // Intrinsic bit manipulation functions
    inline constexpr u32 PopCount32(u32 Value) noexcept;
    inline constexpr u32 PopCount64(u64 Value) noexcept;
    inline constexpr u32 CountLeadingZeros32(u32 Value) noexcept;
    inline constexpr u32 CountLeadingZeros64(u64 Value) noexcept;
    inline constexpr u32 CountTrailingZeros32(u32 Value) noexcept;
    inline constexpr u32 CountTrailingZeros64(u64 Value) noexcept;

    // Memory management
    struct FMemoryAllocator
    {
        virtual void* MemoryAllocate(usize Size, usize Alignment)                 = 0;
        virtual void* MemoryReallocate(void* Ptr, usize NewSize, usize Alignment) = 0;
        virtual void  MemoryFree(void* Ptr)                                       = 0;
    };

} // namespace AltinaEngine::Core::Platform