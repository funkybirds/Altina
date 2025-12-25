#include "../../Public/Threading/Atomic.h"
#include "../../Public/Platform/Generic/GenericPlatformDecl.h"
#include <new>

namespace AltinaEngine::Core::Threading {

using namespace AltinaEngine::Core::Platform::Generic;

struct FAtomicImpl {
    volatile i32 Value;
};

FAtomicInt32::FAtomicInt32(i32 Initial) noexcept {
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

i32 FAtomicInt32::Load() const noexcept {
    FAtomicImpl* P = static_cast<FAtomicImpl*>(Impl);
    return PlatformInterlockedCompareExchange32(&P->Value, 0, 0);
}

void FAtomicInt32::Store(i32 Value) noexcept {
    FAtomicImpl* P = static_cast<FAtomicImpl*>(Impl);
    PlatformInterlockedExchange32(&P->Value, Value);
}

i32 FAtomicInt32::Increment() noexcept {
    FAtomicImpl* P = static_cast<FAtomicImpl*>(Impl);
    return PlatformInterlockedIncrement32(&P->Value);
}

i32 FAtomicInt32::Decrement() noexcept {
    FAtomicImpl* P = static_cast<FAtomicImpl*>(Impl);
    return PlatformInterlockedDecrement32(&P->Value);
}

i32 FAtomicInt32::ExchangeAdd(i32 Delta) noexcept {
    FAtomicImpl* P = static_cast<FAtomicImpl*>(Impl);
    return PlatformInterlockedExchangeAdd32(&P->Value, Delta);
}

i32 FAtomicInt32::CompareExchange(i32 Expected, i32 Desired) noexcept {
    FAtomicImpl* P = static_cast<FAtomicImpl*>(Impl);
    return PlatformInterlockedCompareExchange32(&P->Value, Desired, Expected);
}

i32 FAtomicInt32::Exchange(i32 Desired) noexcept {
    FAtomicImpl* P = static_cast<FAtomicImpl*>(Impl);
    return PlatformInterlockedExchange32(&P->Value, Desired);
}

struct FAtomicImpl64 {
    volatile i64 Value;
};

FAtomicInt64::FAtomicInt64(i64 Initial) noexcept {
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

i64 FAtomicInt64::Load() const noexcept {
    FAtomicImpl64* P = static_cast<FAtomicImpl64*>(Impl);
    return PlatformInterlockedCompareExchange64(&P->Value, 0, 0);
}

void FAtomicInt64::Store(i64 Value) noexcept {
    FAtomicImpl64* P = static_cast<FAtomicImpl64*>(Impl);
    PlatformInterlockedExchange64(&P->Value, Value);
}

i64 FAtomicInt64::Increment() noexcept {
    FAtomicImpl64* P = static_cast<FAtomicImpl64*>(Impl);
    return PlatformInterlockedIncrement64(&P->Value);
}

i64 FAtomicInt64::Decrement() noexcept {
    FAtomicImpl64* P = static_cast<FAtomicImpl64*>(Impl);
    return PlatformInterlockedDecrement64(&P->Value);
}

i64 FAtomicInt64::ExchangeAdd(i64 Delta) noexcept {
    FAtomicImpl64* P = static_cast<FAtomicImpl64*>(Impl);
    return PlatformInterlockedExchangeAdd64(&P->Value, Delta);
}

i64 FAtomicInt64::CompareExchange(i64 Expected, i64 Desired) noexcept {
    FAtomicImpl64* P = static_cast<FAtomicImpl64*>(Impl);
    return PlatformInterlockedCompareExchange64(&P->Value, Desired, Expected);
}

i64 FAtomicInt64::Exchange(i64 Desired) noexcept {
    FAtomicImpl64* P = static_cast<FAtomicImpl64*>(Impl);
    return PlatformInterlockedExchange64(&P->Value, Desired);
}

} // namespace
