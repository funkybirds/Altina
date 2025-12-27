#include "../../Public/Threading/Atomic.h"
#include "../../Public/Platform/Generic/GenericPlatformDecl.h"
#include <new>

namespace AltinaEngine::Core::Threading
{

    using namespace AltinaEngine::Core::Platform::Generic;

    struct FAtomicImpl
    {
        volatile i32 mValue;
    };

    FAtomicInt32::FAtomicInt32(i32 Initial) noexcept
    {
        auto* p   = static_cast<FAtomicImpl*>(operator new(sizeof(FAtomicImpl)));
        p->mValue = Initial;
        mImpl     = p;
    }

    FAtomicInt32::~FAtomicInt32() noexcept
    {
        if (mImpl)
        {
            ::operator delete(mImpl);
            mImpl = nullptr;
        }
    }

    auto FAtomicInt32::Load() const noexcept -> i32
    {
        auto* p = static_cast<FAtomicImpl*>(mImpl);
        return PlatformInterlockedCompareExchange32(&p->mValue, 0, 0);
    }

    void FAtomicInt32::Store(i32 Value) noexcept
    {
        auto* p = static_cast<FAtomicImpl*>(mImpl);
        PlatformInterlockedExchange32(&p->mValue, Value);
    }

    auto FAtomicInt32::Increment() noexcept -> i32
    {
        auto* p = static_cast<FAtomicImpl*>(mImpl);
        return PlatformInterlockedIncrement32(&p->mValue);
    }

    auto FAtomicInt32::Decrement() noexcept -> i32
    {
        auto* p = static_cast<FAtomicImpl*>(mImpl);
        return PlatformInterlockedDecrement32(&p->mValue);
    }

    auto FAtomicInt32::ExchangeAdd(i32 Delta) noexcept -> i32
    {
        auto* p = static_cast<FAtomicImpl*>(mImpl);
        return PlatformInterlockedExchangeAdd32(&p->mValue, Delta);
    }

    auto FAtomicInt32::CompareExchange(i32 Expected, i32 Desired) noexcept -> i32
    {
        auto* p = static_cast<FAtomicImpl*>(mImpl);
        return PlatformInterlockedCompareExchange32(&p->mValue, Desired, Expected);
    }

    auto FAtomicInt32::Exchange(i32 Desired) noexcept -> i32
    {
        auto* p = static_cast<FAtomicImpl*>(mImpl);
        return PlatformInterlockedExchange32(&p->mValue, Desired);
    }

    struct FAtomicImpl64
    {
        volatile i64 mValue;
    };

    FAtomicInt64::FAtomicInt64(i64 Initial) noexcept
    {
        auto* p   = static_cast<FAtomicImpl64*>(::operator new(sizeof(FAtomicImpl64)));
        p->mValue = Initial;
        mImpl     = p;
    }

    FAtomicInt64::~FAtomicInt64() noexcept
    {
        if (mImpl)
        {
            ::operator delete(mImpl);
            mImpl = nullptr;
        }
    }

    auto FAtomicInt64::Load() const noexcept -> i64
    {
        auto* p = static_cast<FAtomicImpl64*>(mImpl);
        return PlatformInterlockedCompareExchange64(&p->mValue, 0, 0);
    }

    void FAtomicInt64::Store(i64 Value) noexcept
    {
        auto* p = static_cast<FAtomicImpl64*>(mImpl);
        PlatformInterlockedExchange64(&p->mValue, Value);
    }

    auto FAtomicInt64::Increment() noexcept -> i64
    {
        auto* p = static_cast<FAtomicImpl64*>(mImpl);
        return PlatformInterlockedIncrement64(&p->mValue);
    }

    auto FAtomicInt64::Decrement() noexcept -> i64
    {
        auto* p = static_cast<FAtomicImpl64*>(mImpl);
        return PlatformInterlockedDecrement64(&p->mValue);
    }

    auto FAtomicInt64::ExchangeAdd(i64 Delta) noexcept -> i64
    {
        auto* p = static_cast<FAtomicImpl64*>(mImpl);
        return PlatformInterlockedExchangeAdd64(&p->mValue, Delta);
    }

    auto FAtomicInt64::CompareExchange(i64 Expected, i64 Desired) noexcept -> i64
    {
        auto* p = static_cast<FAtomicImpl64*>(mImpl);
        return PlatformInterlockedCompareExchange64(&p->mValue, Desired, Expected);
    }

    auto FAtomicInt64::Exchange(i64 Desired) noexcept -> i64
    {
        auto* p = static_cast<FAtomicImpl64*>(mImpl);
        return PlatformInterlockedExchange64(&p->mValue, Desired);
    }

} // namespace AltinaEngine::Core::Threading
