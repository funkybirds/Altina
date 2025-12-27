#pragma once

#include "../../Base/CoreAPI.h"
#include "../../Types/Aliases.h"

namespace AltinaEngine::Core::Platform::Generic
{

    // Opaque platform objects are represented as void* in engine code.

    extern "C"
    {

        // Critical section / mutex
        AE_CORE_API auto PlatformCreateCriticalSection() -> void*;
        AE_CORE_API void PlatformDeleteCriticalSection(void* CS);
        AE_CORE_API void PlatformEnterCriticalSection(void* CS);
        AE_CORE_API auto PlatformTryEnterCriticalSection(void* CS) -> int;
        AE_CORE_API void PlatformLeaveCriticalSection(void* CS);

        // Condition variable
        AE_CORE_API auto PlatformCreateConditionVariable() -> void*;
        AE_CORE_API void PlatformDeleteConditionVariable(void* CV);
        AE_CORE_API void PlatformWakeConditionVariable(void* CV);
        AE_CORE_API void PlatformWakeAllConditionVariable(void* CV);
        AE_CORE_API auto PlatformSleepConditionVariableCS(void* CV, void* CS, unsigned long Milliseconds) -> int;

        // Event
        AE_CORE_API auto PlatformCreateEvent(int bManualReset, int bInitiallySignaled) -> void*;
        AE_CORE_API void PlatformCloseEvent(void* Event);
        AE_CORE_API void PlatformSetEvent(void* Event);
        AE_CORE_API void PlatformResetEvent(void* Event);
        AE_CORE_API auto PlatformWaitForEvent(void* Event, unsigned long Milliseconds) -> int;

        // Interlocked / atomic primitives (operate on integer storage)
        AE_CORE_API auto PlatformInterlockedCompareExchange32(volatile i32* ptr, i32 exchange, i32 comparand) -> i32;
        AE_CORE_API auto PlatformInterlockedExchange32(volatile i32* ptr, i32 value) -> i32;
        AE_CORE_API auto PlatformInterlockedIncrement32(volatile i32* ptr) -> i32;
        AE_CORE_API i32  PlatformInterlockedDecrement32(volatile i32* ptr);
        AE_CORE_API i32  PlatformInterlockedExchangeAdd32(volatile i32* ptr, i32 add);

        AE_CORE_API i64  PlatformInterlockedCompareExchange64(volatile i64* ptr, i64 exchange, i64 comparand);
        AE_CORE_API i64  PlatformInterlockedExchange64(volatile i64* ptr, i64 value);
        AE_CORE_API i64  PlatformInterlockedIncrement64(volatile i64* ptr);
        AE_CORE_API i64  PlatformInterlockedDecrement64(volatile i64* ptr);
        AE_CORE_API i64  PlatformInterlockedExchangeAdd64(volatile i64* ptr, i64 add);

    } // extern "C"

    // Portable helpers in namespace
    AE_CORE_API void PlatformSleepMilliseconds(unsigned long Milliseconds);

    // Abort / terminate / memory helpers
    extern "C"
    {
        AE_CORE_API void  PlatformAbort();
        AE_CORE_API void  PlatformTerminate();
        AE_CORE_API void* Memset(void* Dest, int Value, usize Count);
        AE_CORE_API void* Memcpy(void* Dest, const void* Src, usize Count);
    }

} // namespace AltinaEngine::Core::Platform::Generic
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
        virtual ~FMemoryAllocator()                                               = default;
        virtual void* MemoryAllocate(usize Size, usize Alignment)                 = 0;
        virtual void* MemoryReallocate(void* Ptr, usize NewSize, usize Alignment) = 0;
        virtual void  MemoryFree(void* Ptr)                                       = 0;
    };

    // Accessor for the engine-wide default allocator instance.
    // The implementation lives in the platform memory module and returns
    // a pointer to a static FMemoryAllocator instance.
    [[nodiscard]] AE_CORE_API FMemoryAllocator* GetGlobalMemoryAllocator() noexcept;

} // namespace AltinaEngine::Core::Platform