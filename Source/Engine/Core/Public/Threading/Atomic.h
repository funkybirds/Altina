// Minimal atomic wrappers avoiding std::atomic.
#pragma once

#include "../Base/CoreAPI.h"
#include <type_traits>
#include "../Types/Aliases.h"

namespace AltinaEngine::Core::Threading {

class AE_CORE_API FAtomicInt32 {
public:
    explicit FAtomicInt32(i32 Initial = 0) noexcept;
    ~FAtomicInt32() noexcept;
    i32 Load() const noexcept;
    void Store(i32 Value) noexcept;
    i32 Increment() noexcept;
    i32 Decrement() noexcept;
    i32 ExchangeAdd(i32 Delta) noexcept;
    i32 Exchange(i32 Desired) noexcept;
    i32 CompareExchange(i32 Expected, i32 Desired) noexcept;

    // Non-copyable
    FAtomicInt32(const FAtomicInt32&) = delete;
    FAtomicInt32& operator=(const FAtomicInt32&) = delete;
private:
    mutable void* Impl; // Opaque pointer to platform storage
};

class AE_CORE_API FAtomicInt64 {
public:
    explicit FAtomicInt64(i64 Initial = 0) noexcept;
    ~FAtomicInt64() noexcept;
    i64 Load() const noexcept;
    void Store(i64 Value) noexcept;
    i64 Increment() noexcept;
    i64 Decrement() noexcept;
    i64 ExchangeAdd(i64 Delta) noexcept;
    i64 Exchange(i64 Desired) noexcept;
    i64 CompareExchange(i64 Expected, i64 Desired) noexcept;

    FAtomicInt64(const FAtomicInt64&) = delete;
    FAtomicInt64& operator=(const FAtomicInt64&) = delete;
private:
    mutable void* Impl;
};

} // namespace

// Public templated atomic wrapper previously in Public/Container/Atomic.h
// Moved here to centralize atomic types alongside the engine atomics.
namespace AltinaEngine::Core::Threading
{
    enum class EMemoryOrder
    {
        Relaxed,
        Consume,
        Acquire,
        Release,
        AcquireRelease,
        SequentiallyConsistent,
    };

    template <typename T>
    class TAtomic
    {
    public:
        using value_type = T;

        static_assert(std::is_integral_v<T>, "TAtomic currently supports integral types only");
        static_assert(sizeof(T) == 4 || sizeof(T) == 8, "TAtomic supports 32-bit and 64-bit integral types only");

        using ImplType = std::conditional_t<sizeof(T) == 4, FAtomicInt32, FAtomicInt64>;
        using SignedType = std::conditional_t<sizeof(T) == 4, i32, i64>;

        constexpr TAtomic() noexcept : mImpl(static_cast<SignedType>(0)) {}
        constexpr explicit TAtomic(T desired) noexcept : mImpl(static_cast<SignedType>(desired)) {}

        TAtomic(const TAtomic&) = delete;
        TAtomic& operator=(const TAtomic&) = delete;

        [[nodiscard]] bool IsLockFree() const noexcept { return true; }

        void Store(T desired, EMemoryOrder = EMemoryOrder::SequentiallyConsistent) noexcept
        {
            mImpl.Store(static_cast<SignedType>(desired));
        }

        T Load(EMemoryOrder = EMemoryOrder::SequentiallyConsistent) const noexcept
        {
            return static_cast<T>(mImpl.Load());
        }

        T Exchange(T desired, EMemoryOrder = EMemoryOrder::SequentiallyConsistent) noexcept
        {
            return static_cast<T>(mImpl.Exchange(static_cast<SignedType>(desired)));
        }

        bool CompareExchangeWeak(T& expected, T desired,
                                 EMemoryOrder = EMemoryOrder::SequentiallyConsistent,
                                 EMemoryOrder = EMemoryOrder::SequentiallyConsistent) noexcept
        {
            SignedType exp = static_cast<SignedType>(expected);
            SignedType prev = mImpl.CompareExchange(exp, static_cast<SignedType>(desired));
            if (prev == exp) return true;
            expected = static_cast<T>(prev);
            return false;
        }

        bool CompareExchangeStrong(T& expected, T desired,
                                   EMemoryOrder success = EMemoryOrder::SequentiallyConsistent,
                                   EMemoryOrder failure = EMemoryOrder::SequentiallyConsistent) noexcept
        {
            return CompareExchangeWeak(expected, desired, success, failure);
        }

        T operator=(T desired) noexcept
        {
            Store(desired);
            return desired;
        }

        [[nodiscard]] operator T() const noexcept { return Load(); }

        template <typename U = T, typename = std::enable_if_t<std::is_integral_v<U>>>
        U FetchAdd(U arg, EMemoryOrder = EMemoryOrder::SequentiallyConsistent) noexcept
        {
            return static_cast<U>(mImpl.ExchangeAdd(static_cast<SignedType>(arg)));
        }

        template <typename U = T, typename = std::enable_if_t<std::is_integral_v<U>>>
        U FetchSub(U arg, EMemoryOrder = EMemoryOrder::SequentiallyConsistent) noexcept
        {
            return static_cast<U>(mImpl.ExchangeAdd(-static_cast<SignedType>(arg)));
        }

        template <typename U = T, typename = std::enable_if_t<std::is_integral_v<U>>>
        U FetchAnd(U arg, EMemoryOrder = EMemoryOrder::SequentiallyConsistent) noexcept
        {
            SignedType expected, desired;
            do {
                expected = mImpl.Load();
                desired = expected & static_cast<SignedType>(arg);
            } while (mImpl.CompareExchange(expected, desired) != expected);
            return static_cast<U>(expected);
        }

        template <typename U = T, typename = std::enable_if_t<std::is_integral_v<U>>>
        U FetchOr(U arg, EMemoryOrder = EMemoryOrder::SequentiallyConsistent) noexcept
        {
            SignedType expected, desired;
            do {
                expected = mImpl.Load();
                desired = expected | static_cast<SignedType>(arg);
            } while (mImpl.CompareExchange(expected, desired) != expected);
            return static_cast<U>(expected);
        }

        template <typename U = T, typename = std::enable_if_t<std::is_integral_v<U>>>
        U FetchXor(U arg, EMemoryOrder = EMemoryOrder::SequentiallyConsistent) noexcept
        {
            SignedType expected, desired;
            do {
                expected = mImpl.Load();
                desired = expected ^ static_cast<SignedType>(arg);
            } while (mImpl.CompareExchange(expected, desired) != expected);
            return static_cast<U>(expected);
        }

    private:
        ImplType mImpl;
    };

} // namespace AltinaEngine::Core::Threading
