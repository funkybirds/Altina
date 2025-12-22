#pragma once

#include <atomic>

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

    inline constexpr std::memory_order ToStdMemoryOrder(EMemoryOrder order)
    {
        switch (order)
        {
            case EMemoryOrder::Relaxed:
                return std::memory_order_relaxed;
            case EMemoryOrder::Consume:
                return std::memory_order_consume;
            case EMemoryOrder::Acquire:
                return std::memory_order_acquire;
            case EMemoryOrder::Release:
                return std::memory_order_release;
            case EMemoryOrder::AcquireRelease:
                return std::memory_order_acq_rel;
            case EMemoryOrder::SequentiallyConsistent:
            default:
                return std::memory_order_seq_cst;
        }
    }

    // Thin wrapper over std::atomic that keeps engine naming consistent.
    template <typename T>
    class TAtomic
    {
    public:
        using value_type  = T;
        using atomic_type = std::atomic<T>;

        constexpr TAtomic() noexcept = default;
        constexpr explicit TAtomic(T desired) noexcept : mValue(desired) {}

        TAtomic(const TAtomic&) = delete;
        TAtomic& operator=(const TAtomic&) = delete;

        [[nodiscard]] bool IsLockFree() const noexcept { return mValue.is_lock_free(); }

        void Store(T desired,
                   EMemoryOrder order = EMemoryOrder::SequentiallyConsistent) noexcept
        {
            mValue.store(desired, ToStdMemoryOrder(order));
        }

        T Load(EMemoryOrder order = EMemoryOrder::SequentiallyConsistent) const noexcept
        {
            return mValue.load(ToStdMemoryOrder(order));
        }

        T Exchange(T desired, EMemoryOrder order = EMemoryOrder::SequentiallyConsistent) noexcept
        {
            return mValue.exchange(desired, ToStdMemoryOrder(order));
        }

        bool CompareExchangeWeak(T& expected, T desired,
                                 EMemoryOrder success = EMemoryOrder::SequentiallyConsistent,
                                 EMemoryOrder failure = EMemoryOrder::SequentiallyConsistent) noexcept
        {
            return mValue.compare_exchange_weak(expected, desired, ToStdMemoryOrder(success),
                                                ToStdMemoryOrder(failure));
        }

        bool CompareExchangeStrong(T& expected, T desired,
                                   EMemoryOrder success = EMemoryOrder::SequentiallyConsistent,
                                   EMemoryOrder failure = EMemoryOrder::SequentiallyConsistent) noexcept
        {
            return mValue.compare_exchange_strong(expected, desired, ToStdMemoryOrder(success),
                                                  ToStdMemoryOrder(failure));
        }

        T operator=(T desired) noexcept
        {
            Store(desired);
            return desired;
        }

        [[nodiscard]] operator T() const noexcept { return Load(); }

        template <typename U = T, typename = std::enable_if_t<std::is_integral_v<U>>>
        U FetchAdd(U arg, EMemoryOrder order = EMemoryOrder::SequentiallyConsistent) noexcept
        {
            return mValue.fetch_add(arg, ToStdMemoryOrder(order));
        }

        template <typename U = T, typename = std::enable_if_t<std::is_integral_v<U>>>
        U FetchSub(U arg, EMemoryOrder order = EMemoryOrder::SequentiallyConsistent) noexcept
        {
            return mValue.fetch_sub(arg, ToStdMemoryOrder(order));
        }

        template <typename U = T, typename = std::enable_if_t<std::is_integral_v<U>>>
        U FetchAnd(U arg, EMemoryOrder order = EMemoryOrder::SequentiallyConsistent) noexcept
        {
            return mValue.fetch_and(arg, ToStdMemoryOrder(order));
        }

        template <typename U = T, typename = std::enable_if_t<std::is_integral_v<U>>>
        U FetchOr(U arg, EMemoryOrder order = EMemoryOrder::SequentiallyConsistent) noexcept
        {
            return mValue.fetch_or(arg, ToStdMemoryOrder(order));
        }

        template <typename U = T, typename = std::enable_if_t<std::is_integral_v<U>>>
        U FetchXor(U arg, EMemoryOrder order = EMemoryOrder::SequentiallyConsistent) noexcept
        {
            return mValue.fetch_xor(arg, ToStdMemoryOrder(order));
        }

    private:
        atomic_type mValue;
    };

} // namespace AltinaEngine::Core::Container
