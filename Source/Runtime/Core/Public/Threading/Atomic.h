// Minimal atomic wrappers avoiding std::atomic.
#pragma once

#include "../Base/CoreAPI.h"
#include "../Types/Traits.h"
#include "../Types/Aliases.h"

namespace AltinaEngine::Core::Threading {

    class AE_CORE_API FAtomicInt32 {
    public:
        explicit FAtomicInt32(i32 Initial = 0) noexcept;
        ~FAtomicInt32() noexcept;
        auto Load() const noexcept -> i32;
        void Store(i32 Value) noexcept;
        auto Increment() noexcept -> i32;
        auto Decrement() noexcept -> i32;
        auto ExchangeAdd(i32 Delta) noexcept -> i32;
        auto Exchange(i32 Desired) noexcept -> i32;
        auto CompareExchange(i32 Expected, i32 Desired) noexcept -> i32;

        // Non-copyable
        FAtomicInt32(const FAtomicInt32&)                    = delete;
        auto operator=(const FAtomicInt32&) -> FAtomicInt32& = delete;
        FAtomicInt32(FAtomicInt32&&) noexcept;
        auto operator=(FAtomicInt32&&) noexcept -> FAtomicInt32&;

    private:
        mutable void* mImpl; // Opaque pointer to platform storage
    };

    class AE_CORE_API FAtomicInt64 {
    public:
        explicit FAtomicInt64(i64 Initial = 0) noexcept;
        ~FAtomicInt64() noexcept;
        auto Load() const noexcept -> i64;
        void Store(i64 Value) noexcept;
        auto Increment() noexcept -> i64;
        auto Decrement() noexcept -> i64;
        auto ExchangeAdd(i64 Delta) noexcept -> i64;
        auto Exchange(i64 Desired) noexcept -> i64;
        auto CompareExchange(i64 Expected, i64 Desired) noexcept -> i64;

        FAtomicInt64(const FAtomicInt64&)                    = delete;
        auto operator=(const FAtomicInt64&) -> FAtomicInt64& = delete;
        FAtomicInt64(FAtomicInt64&&) noexcept;
        auto operator=(FAtomicInt64&&) noexcept -> FAtomicInt64&;

    private:
        mutable void* mImpl;
    };

} // namespace AltinaEngine::Core::Threading

// Public templated atomic wrapper previously in Public/Container/Atomic.h
// Moved here to centralize atomic types alongside the engine atomics.
namespace AltinaEngine::Core::Threading {
    enum class EMemoryOrder : u8 {
        Relaxed,
        Consume,
        Acquire,
        Release,
        AcquireRelease,
        SequentiallyConsistent,
    };

    template <typename T> class TAtomic {
    public:
        using TValueType = T;

        static_assert(CIntegral<T>, "TAtomic currently supports integral types only");
        static_assert(sizeof(T) == 4 || sizeof(T) == 8,
            "TAtomic supports 32-bit and 64-bit integral types only");

        template <usize N> struct ImplForSize;
        template <> struct ImplForSize<4> {
            using Type = FAtomicInt32; // NOLINT(*-identifier-naming)
        };
        template <> struct ImplForSize<8> {
            using Type = FAtomicInt64; // NOLINT(*-identifier-naming)
        };

        template <usize N> struct SignedForSize;
        template <> struct SignedForSize<4> {
            using Type = i32; // NOLINT(*-identifier-naming)
        };
        template <> struct SignedForSize<8> {
            using Type = i64; // NOLINT(*-identifier-naming)
        };

        using TImplType   = ImplForSize<sizeof(T)>::Type;
        using TSignedType = SignedForSize<sizeof(T)>::Type;

        constexpr TAtomic() noexcept : mImpl(static_cast<TSignedType>(0)) {}
        constexpr explicit TAtomic(T desired) noexcept : mImpl(static_cast<TSignedType>(desired)) {}

        TAtomic(const TAtomic&)                    = delete;
        auto operator=(const TAtomic&) -> TAtomic& = delete;
        TAtomic(TAtomic&& other) noexcept : mImpl(AltinaEngine::Move(other.mImpl)) {}
        auto operator=(TAtomic&& other) noexcept -> TAtomic& {
            if (this != &other) {
                mImpl = AltinaEngine::Move(other.mImpl);
            }
            return *this;
        }

        [[nodiscard]] auto IsLockFree() const noexcept -> bool { return true; }

        void Store(T desired, EMemoryOrder = EMemoryOrder::SequentiallyConsistent) noexcept {
            mImpl.Store(static_cast<TSignedType>(desired));
        }

        auto Load(EMemoryOrder = EMemoryOrder::SequentiallyConsistent) const noexcept -> T {
            return static_cast<T>(mImpl.Load());
        }

        auto Exchange(T desired, EMemoryOrder = EMemoryOrder::SequentiallyConsistent) noexcept
            -> T {
            return static_cast<T>(mImpl.Exchange(static_cast<TSignedType>(desired)));
        }

        auto CompareExchangeWeak(T& expected, T desired,
            EMemoryOrder = EMemoryOrder::SequentiallyConsistent,
            EMemoryOrder = EMemoryOrder::SequentiallyConsistent) noexcept -> bool {
            auto        exp  = static_cast<TSignedType>(expected);
            TSignedType prev = mImpl.CompareExchange(exp, static_cast<TSignedType>(desired));
            if (prev == exp)
                return true;
            expected = static_cast<T>(prev);
            return false;
        }

        auto CompareExchangeStrong(T& expected, T desired,
            EMemoryOrder success = EMemoryOrder::SequentiallyConsistent,
            EMemoryOrder failure = EMemoryOrder::SequentiallyConsistent) noexcept -> bool {
            return CompareExchangeWeak(expected, desired, success, failure);
        }

        auto operator=(T desired) noexcept -> T {
            Store(desired);
            return desired;
        }

        [[nodiscard]] operator T() const noexcept { return Load(); }

        template <typename U = T>
            requires(CIntegral<U>)
        auto FetchAdd(U arg, EMemoryOrder = EMemoryOrder::SequentiallyConsistent) noexcept -> U {
            return static_cast<U>(mImpl.ExchangeAdd(static_cast<TSignedType>(arg)));
        }

        template <typename U = T>
            requires(CIntegral<U>)
        auto FetchSub(U arg, EMemoryOrder = EMemoryOrder::SequentiallyConsistent) noexcept -> U {
            return static_cast<U>(mImpl.ExchangeAdd(-static_cast<TSignedType>(arg)));
        }

        template <typename U = T>
            requires(CIntegral<U>)
        auto FetchAnd(U arg, EMemoryOrder = EMemoryOrder::SequentiallyConsistent) noexcept -> U {
            TSignedType expected, desired;
            do {
                expected = mImpl.Load();
                desired  = expected & static_cast<TSignedType>(arg);
            } while (mImpl.CompareExchange(expected, desired) != expected);
            return static_cast<U>(expected);
        }

        template <typename U = T>
            requires(CIntegral<U>)
        auto FetchOr(U arg, EMemoryOrder = EMemoryOrder::SequentiallyConsistent) noexcept -> U {
            TSignedType expected, desired;
            do {
                expected = mImpl.Load();
                desired  = expected | static_cast<TSignedType>(arg);
            } while (mImpl.CompareExchange(expected, desired) != expected);
            return static_cast<U>(expected);
        }

        template <typename U = T>
            requires(CIntegral<U>)
        auto FetchXor(U arg, EMemoryOrder = EMemoryOrder::SequentiallyConsistent) noexcept -> U {
            TSignedType expected, desired;
            do {
                expected = mImpl.Load();
                desired  = expected ^ static_cast<TSignedType>(arg);
            } while (mImpl.CompareExchange(expected, desired) != expected);
            return static_cast<U>(expected);
        }

    private:
        TImplType mImpl;
    };

} // namespace AltinaEngine::Core::Threading
