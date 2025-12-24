#pragma once

#include <cstdint>
#include <type_traits>
#include "../Threading/Atomic.h"

namespace AltinaEngine::Core::Container
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

    // Thin wrapper that uses engine atomics for supported types.
    template <typename T>
    class TAtomic
    {
    public:
        using value_type = T;

        static_assert(std::is_integral_v<T>, "TAtomic currently supports integral types only");
        static_assert(sizeof(T) == 4 || sizeof(T) == 8, "TAtomic supports 32-bit and 64-bit integral types only");

        using ImplType = std::conditional_t<sizeof(T) == 4,
                                           AltinaEngine::Core::Threading::FAtomicInt32,
                                           AltinaEngine::Core::Threading::FAtomicInt64>;

        constexpr TAtomic() noexcept : mImpl(0) {}
        constexpr explicit TAtomic(T desired) noexcept : mImpl(static_cast<std::conditional_t<sizeof(T)==4,int32_t,int64_t>>(desired)) {}

        TAtomic(const TAtomic&) = delete;
        TAtomic& operator=(const TAtomic&) = delete;

        [[nodiscard]] bool IsLockFree() const noexcept { return true; }

        void Store(T desired, EMemoryOrder = EMemoryOrder::SequentiallyConsistent) noexcept
        {
            if constexpr (sizeof(T) == 4)
                mImpl.Store(static_cast<int32_t>(desired));
            else
                mImpl.Store(static_cast<int64_t>(desired));
        }

        T Load(EMemoryOrder = EMemoryOrder::SequentiallyConsistent) const noexcept
        {
            if constexpr (sizeof(T) == 4)
                return static_cast<T>(mImpl.Load());
            else
                return static_cast<T>(mImpl.Load());
        }

        T Exchange(T desired, EMemoryOrder = EMemoryOrder::SequentiallyConsistent) noexcept
        {
            if constexpr (sizeof(T) == 4)
                return static_cast<T>(mImpl.Exchange(static_cast<int32_t>(desired)));
            else
                return static_cast<T>(mImpl.Exchange(static_cast<int64_t>(desired)));
        }

        bool CompareExchangeWeak(T& expected, T desired,
                                 EMemoryOrder = EMemoryOrder::SequentiallyConsistent,
                                 EMemoryOrder = EMemoryOrder::SequentiallyConsistent) noexcept
        {
            if constexpr (sizeof(T) == 4) {
                int32_t exp = static_cast<int32_t>(expected);
                int32_t prev = mImpl.CompareExchange(exp, static_cast<int32_t>(desired));
                if (prev == exp) return true;
                expected = static_cast<T>(prev);
                return false;
            } else {
                int64_t exp = static_cast<int64_t>(expected);
                int64_t prev = mImpl.CompareExchange(exp, static_cast<int64_t>(desired));
                if (prev == exp) return true;
                expected = static_cast<T>(prev);
                return false;
            }
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
            if constexpr (sizeof(T) == 4)
                return static_cast<U>(mImpl.ExchangeAdd(static_cast<int32_t>(arg)));
            else
                return static_cast<U>(mImpl.ExchangeAdd(static_cast<int64_t>(arg)));
        }

        template <typename U = T, typename = std::enable_if_t<std::is_integral_v<U>>>
        U FetchSub(U arg, EMemoryOrder = EMemoryOrder::SequentiallyConsistent) noexcept
        {
            if constexpr (sizeof(T) == 4)
                return static_cast<U>(mImpl.ExchangeAdd(-static_cast<int32_t>(arg)));
            else
                return static_cast<U>(mImpl.ExchangeAdd(-static_cast<int64_t>(arg)));
        }

        template <typename U = T, typename = std::enable_if_t<std::is_integral_v<U>>>
        U FetchAnd(U arg, EMemoryOrder = EMemoryOrder::SequentiallyConsistent) noexcept
        {
            if constexpr (sizeof(T) == 4) {
                int32_t expected, desired;
                do {
                    expected = mImpl.Load();
                    desired = expected & static_cast<int32_t>(arg);
                } while (mImpl.CompareExchange(expected, desired) != expected);
                return static_cast<U>(expected);
            } else {
                int64_t expected, desired;
                do {
                    expected = mImpl.Load();
                    desired = expected & static_cast<int64_t>(arg);
                } while (mImpl.CompareExchange(expected, desired) != expected);
                return static_cast<U>(expected);
            }
        }

        template <typename U = T, typename = std::enable_if_t<std::is_integral_v<U>>>
        U FetchOr(U arg, EMemoryOrder = EMemoryOrder::SequentiallyConsistent) noexcept
        {
            if constexpr (sizeof(T) == 4) {
                int32_t expected, desired;
                do {
                    expected = mImpl.Load();
                    desired = expected | static_cast<int32_t>(arg);
                } while (mImpl.CompareExchange(expected, desired) != expected);
                return static_cast<U>(expected);
            } else {
                int64_t expected, desired;
                do {
                    expected = mImpl.Load();
                    desired = expected | static_cast<int64_t>(arg);
                } while (mImpl.CompareExchange(expected, desired) != expected);
                return static_cast<U>(expected);
            }
        }

        template <typename U = T, typename = std::enable_if_t<std::is_integral_v<U>>>
        U FetchXor(U arg, EMemoryOrder = EMemoryOrder::SequentiallyConsistent) noexcept
        {
            if constexpr (sizeof(T) == 4) {
                int32_t expected, desired;
                do {
                    expected = mImpl.Load();
                    desired = expected ^ static_cast<int32_t>(arg);
                } while (mImpl.CompareExchange(expected, desired) != expected);
                return static_cast<U>(expected);
            } else {
                int64_t expected, desired;
                do {
                    expected = mImpl.Load();
                    desired = expected ^ static_cast<int64_t>(arg);
                } while (mImpl.CompareExchange(expected, desired) != expected);
                return static_cast<U>(expected);
            }
        }

    private:
        ImplType mImpl;
    };

} // namespace AltinaEngine::Core::Container
