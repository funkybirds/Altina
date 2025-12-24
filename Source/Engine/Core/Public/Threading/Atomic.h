// Minimal atomic wrappers avoiding std::atomic.
#pragma once

#include <cstdint>
#include "../Base/CoreAPI.h"

namespace AltinaEngine::Core::Threading {

class AE_CORE_API FAtomicInt32 {
public:
    explicit FAtomicInt32(int32_t Initial = 0) noexcept;
    ~FAtomicInt32() noexcept;
    int32_t Load() const noexcept;
    void Store(int32_t Value) noexcept;
    int32_t Increment() noexcept;
    int32_t Decrement() noexcept;
    int32_t ExchangeAdd(int32_t Delta) noexcept;
    int32_t Exchange(int32_t Desired) noexcept;
    int32_t CompareExchange(int32_t Expected, int32_t Desired) noexcept;

    // Non-copyable
    FAtomicInt32(const FAtomicInt32&) = delete;
    FAtomicInt32& operator=(const FAtomicInt32&) = delete;
private:
    mutable void* Impl; // Opaque pointer to platform storage
};

class AE_CORE_API FAtomicInt64 {
public:
    explicit FAtomicInt64(int64_t Initial = 0) noexcept;
    ~FAtomicInt64() noexcept;
    int64_t Load() const noexcept;
    void Store(int64_t Value) noexcept;
    int64_t Increment() noexcept;
    int64_t Decrement() noexcept;
    int64_t ExchangeAdd(int64_t Delta) noexcept;
    int64_t Exchange(int64_t Desired) noexcept;
    int64_t CompareExchange(int64_t Expected, int64_t Desired) noexcept;

    FAtomicInt64(const FAtomicInt64&) = delete;
    FAtomicInt64& operator=(const FAtomicInt64&) = delete;
private:
    mutable void* Impl;
};

} // namespace
