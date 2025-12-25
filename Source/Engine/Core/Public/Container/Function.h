#pragma once

// Minimal TFunction - a small std::function-like wrapper without using STL
// Features: type-erased callable, small-buffer optimization, copy/move, invoke

#include "../Base/CoreAPI.h"
#include "../Types/Aliases.h"

#include <type_traits>
#include <utility>
#include <new>
#include <cstring>

namespace AltinaEngine::Core::Container {

template <typename> class TFunction;

template <typename R, typename... Args>
class TFunction<R(Args...)>
{
public:
    using ResultType = R;

    TFunction() noexcept : vtable(nullptr) {}

    // Construct from callable (disable for TFunction itself)
    template <typename F, typename = std::enable_if_t<!std::is_same_v<std::decay_t<F>, TFunction<R(Args...)>>>>
    TFunction(F&& f) noexcept {
        using DecayF = std::remove_reference_t<F>;
        init<DecayF>(static_cast<DecayF&&>(f));
    }

    TFunction(const TFunction& other) : vtable(nullptr) {
        if (other.vtable && other.vtable->copy) {
            other.vtable->copy(&other.storage, &storage);
            vtable = other.vtable;
        }
    }

    TFunction(TFunction&& other) noexcept {
        auto tmp = other.vtable;
        other.vtable = nullptr;
        if (tmp) {
            tmp->move(&other.storage, &storage);
            vtable = tmp;
        } else {
            vtable = nullptr;
        }
        // Clear moved-from storage memory to avoid stale bits
        std::memset(&other.storage, 0, sizeof(StorageT));
    }

    TFunction& operator=(const TFunction& other) {
        if (this == &other) return *this;
        reset();
        if (other.vtable && other.vtable->copy) {
            other.vtable->copy(&other.storage, &storage);
            vtable = other.vtable;
        }
        return *this;
    }

    TFunction& operator=(TFunction&& other) noexcept {
        if (this == &other) return *this;
        reset();
        auto tmp = other.vtable;
        other.vtable = nullptr;
        if (tmp) {
            tmp->move(&other.storage, &storage);
            vtable = tmp;
        } else {
            vtable = nullptr;
        }
        std::memset(&other.storage, 0, sizeof(StorageT));
        return *this;
    }

    ~TFunction() { reset(); }

    explicit operator bool() const noexcept { return vtable != nullptr; }

    R operator()(Args... args) {
        return vtable->invoke(&storage, static_cast<Args&&>(args)...);
    }

    void reset() noexcept {
        if (vtable) vtable->destroy(&storage);
        vtable = nullptr;
    }

private:
    // Small buffer capacity (in bytes)
    static constexpr usize SmallSize = 3 * sizeof(void*);
    static constexpr usize SmallAlign = alignof(void*);

    struct StorageT { alignas(SmallAlign) unsigned char data[SmallSize]; };

    struct VTable {
        void (*destroy)(void* storage) noexcept;
        void (*copy)(const void* src, void* dst);
        void (*move)(void* src, void* dst) noexcept;
        R (*invoke)(void* storage, Args... args);
    };

    template <typename F>
    static void destroy_impl(void* storage) noexcept {
        reinterpret_cast<F*>(storage)->~F();
    }

    template <typename F>
    static void copy_impl(const void* src, void* dst) {
        if constexpr (std::is_copy_constructible_v<F>) {
            new (dst) F(*reinterpret_cast<const F*>(src));
        } else {
            // Should not be called when type is non-copyable; guard at vtable level.
            std::terminate();
        }
    }

    template <typename F>
    static void move_impl(void* src, void* dst) noexcept {
        new (dst) F(std::move(*reinterpret_cast<F*>(src)));
        reinterpret_cast<F*>(src)->~F();
    }

    template <typename F>
    static R invoke_impl(void* storage, Args... args) {
        return (*reinterpret_cast<F*>(storage))(static_cast<Args&&>(args)...);
    }

    template <typename F>
    void init(F&& f) noexcept {
        using T = std::remove_reference_t<F>;
        static const VTable vt = {
            &destroy_impl<T>,
            (std::is_copy_constructible_v<T> ? &copy_impl<T> : nullptr),
            &move_impl<T>,
            &invoke_impl<T>
        };

        // Placement new into storage
        new (&storage) T(static_cast<F&&>(f));
        vtable = &vt;
    }

    VTable const* vtable;
    StorageT storage;
};

} // namespace
