#include "../../Public/Threading/Atomic.h"
#include "../../Public/Platform/Generic/GenericPlatformDecl.h"
#include <new>

namespace AltinaEngine::Core::Threading {

using namespace AltinaEngine::Core::Platform::Generic;

struct FAtomicImpl {
    volatile int32_t Value;
};

FAtomicInt32::FAtomicInt32(int32_t Initial) noexcept {
    FAtomicImpl* P = static_cast<FAtomicImpl*>(::operator new(sizeof(FAtomicImpl)));
    P->Value = Initial;
    Impl = P;
}

FAtomicInt32::~FAtomicInt32() noexcept {
    if (Impl) {
        ::operator delete(Impl);
        Impl = nullptr;
    }
}

int32_t FAtomicInt32::Load() const noexcept {
    FAtomicImpl* P = static_cast<FAtomicImpl*>(Impl);
    return PlatformInterlockedCompareExchange32(&P->Value, 0, 0);
}

void FAtomicInt32::Store(int32_t Value) noexcept {
    FAtomicImpl* P = static_cast<FAtomicImpl*>(Impl);
    PlatformInterlockedExchange32(&P->Value, Value);
}

int32_t FAtomicInt32::Increment() noexcept {
    FAtomicImpl* P = static_cast<FAtomicImpl*>(Impl);
    return PlatformInterlockedIncrement32(&P->Value);
}

int32_t FAtomicInt32::Decrement() noexcept {
    FAtomicImpl* P = static_cast<FAtomicImpl*>(Impl);
    return PlatformInterlockedDecrement32(&P->Value);
}

int32_t FAtomicInt32::ExchangeAdd(int32_t Delta) noexcept {
    FAtomicImpl* P = static_cast<FAtomicImpl*>(Impl);
    return PlatformInterlockedExchangeAdd32(&P->Value, Delta);
}

int32_t FAtomicInt32::CompareExchange(int32_t Expected, int32_t Desired) noexcept {
    FAtomicImpl* P = static_cast<FAtomicImpl*>(Impl);
    return PlatformInterlockedCompareExchange32(&P->Value, Desired, Expected);
}

int32_t FAtomicInt32::Exchange(int32_t Desired) noexcept {
    FAtomicImpl* P = static_cast<FAtomicImpl*>(Impl);
    return PlatformInterlockedExchange32(&P->Value, Desired);
}

struct FAtomicImpl64 {
    volatile int64_t Value;
};

FAtomicInt64::FAtomicInt64(int64_t Initial) noexcept {
    FAtomicImpl64* P = static_cast<FAtomicImpl64*>(::operator new(sizeof(FAtomicImpl64)));
    P->Value = Initial;
    Impl = P;
}

FAtomicInt64::~FAtomicInt64() noexcept {
    if (Impl) {
        ::operator delete(Impl);
        Impl = nullptr;
    }
}

int64_t FAtomicInt64::Load() const noexcept {
    FAtomicImpl64* P = static_cast<FAtomicImpl64*>(Impl);
    return PlatformInterlockedCompareExchange64(&P->Value, 0, 0);
}

void FAtomicInt64::Store(int64_t Value) noexcept {
    FAtomicImpl64* P = static_cast<FAtomicImpl64*>(Impl);
    PlatformInterlockedExchange64(&P->Value, Value);
}

int64_t FAtomicInt64::Increment() noexcept {
    FAtomicImpl64* P = static_cast<FAtomicImpl64*>(Impl);
    return PlatformInterlockedIncrement64(&P->Value);
}

int64_t FAtomicInt64::Decrement() noexcept {
    FAtomicImpl64* P = static_cast<FAtomicImpl64*>(Impl);
    return PlatformInterlockedDecrement64(&P->Value);
}

int64_t FAtomicInt64::ExchangeAdd(int64_t Delta) noexcept {
    FAtomicImpl64* P = static_cast<FAtomicImpl64*>(Impl);
    return PlatformInterlockedExchangeAdd64(&P->Value, Delta);
}

int64_t FAtomicInt64::CompareExchange(int64_t Expected, int64_t Desired) noexcept {
    FAtomicImpl64* P = static_cast<FAtomicImpl64*>(Impl);
    return PlatformInterlockedCompareExchange64(&P->Value, Desired, Expected);
}

int64_t FAtomicInt64::Exchange(int64_t Desired) noexcept {
    FAtomicImpl64* P = static_cast<FAtomicImpl64*>(Impl);
    return PlatformInterlockedExchange64(&P->Value, Desired);
}

} // namespace
