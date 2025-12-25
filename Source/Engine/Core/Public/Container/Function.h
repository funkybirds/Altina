#pragma once

// Minimal TFunction - a small std::function-like wrapper without using STL
// Features: type-erased callable, small-buffer optimization, copy/move, invoke

#include "../Base/CoreAPI.h"
#include "../Types/Aliases.h"

#include "../Types/Traits.h"
#include "../Types/Concepts.h"
#include "../Platform/Generic/GenericPlatformDecl.h"
// uses engine type traits from Types/Traits.h and Concepts.h

namespace AltinaEngine::Core::Container {

template <typename> class TFunction;

template <typename R, typename... Args>
class TFunction<R(Args...)>
{
public:
    using ResultType = R;

    TFunction() noexcept : vtable(nullptr) {}

    // Construct from callable (disable for TFunction itself)
    template <typename F>
        requires (!AltinaEngine::TTypeSameAs_v<typename AltinaEngine::TDecay<F>::Type, TFunction<R(Args...)>>)
    TFunction(F&& f) noexcept {
        using DecayF = typename AltinaEngine::TRemoveReference<F>::Type;
        Init<DecayF>(AltinaEngine::Move(f));
    }

    TFunction(const TFunction& other) : vtable(nullptr) {
        if (other.vtable && other.vtable->Copy) {
            other.vtable->Copy(&other.storage, &storage);
            vtable = other.vtable;
        }
    }

    TFunction(TFunction&& other) noexcept {
        auto tmp = other.vtable;
        other.vtable = nullptr;
        if (tmp) {
            tmp->Move(&other.storage, &storage);
            vtable = tmp;
        } else {
            vtable = nullptr;
        }
        // Clear moved-from storage memory to avoid stale bits
        AltinaEngine::Core::Platform::Generic::Memset(&other.storage, 0, sizeof(StorageT));
    }

    TFunction& operator=(const TFunction& other) {
        if (this == &other) return *this;
        Reset();
        if (other.vtable && other.vtable->Copy) {
            other.vtable->Copy(&other.storage, &storage);
            vtable = other.vtable;
        }
        return *this;
    }

    TFunction& operator=(TFunction&& other) noexcept {
        if (this == &other) return *this;
        Reset();
        auto tmp = other.vtable;
        other.vtable = nullptr;
        if (tmp) {
            tmp->Move(&other.storage, &storage);
            vtable = tmp;
        } else {
            vtable = nullptr;
        }
            AltinaEngine::Core::Platform::Generic::Memset(&other.storage, 0, sizeof(StorageT));
        return *this;
    }

    ~TFunction() { Reset(); }

    explicit operator bool() const noexcept { return vtable != nullptr; }

    R operator()(Args... args) {
        return vtable->Invoke(&storage, AltinaEngine::Forward<Args>(args)...);
    }

    void Reset() noexcept {
        if (vtable) vtable->Destroy(&storage);
        vtable = nullptr;
    }

private:
    // Small buffer capacity (in bytes)
    static constexpr usize SmallSize = 3 * sizeof(void*);
    static constexpr usize SmallAlign = alignof(void*);

    struct StorageT { alignas(SmallAlign) unsigned char data[SmallSize]; };

    struct VTable {
        void (*Destroy)(void* storage) noexcept;
        void (*Copy)(const void* src, void* dst);
        void (*Move)(void* src, void* dst) noexcept;
        R (*Invoke)(void* storage, Args... args);
    };

    template <typename F>
    static void DestroyImpl(void* storage) noexcept {
        reinterpret_cast<F*>(storage)->~F();
    }

    template <typename F>
    static void CopyImpl(const void* src, void* dst) {
        if constexpr (AltinaEngine::TTypeIsCopyConstructible_v<F>) {
            new (dst) F(*reinterpret_cast<const F*>(src));
        } else {
            // Should not be called when type is non-copyable; guard at vtable level.
            AltinaEngine::Core::Platform::Generic::PlatformTerminate();
        }
    }

    template <typename F>
    static void MoveImpl(void* src, void* dst) noexcept {
        new (dst) F(AltinaEngine::Move(*reinterpret_cast<F*>(src)));
        reinterpret_cast<F*>(src)->~F();
    }

    template <typename F>
    static R InvokeImpl(void* storage, Args... args) {
        return (*reinterpret_cast<F*>(storage))(AltinaEngine::Forward<Args>(args)...);
    }

    template <typename F>
    void Init(F&& f) noexcept {
        using T = typename AltinaEngine::TRemoveReference<F>::Type;
        static const VTable vt = {
            &DestroyImpl<T>,
            (AltinaEngine::TTypeIsCopyConstructible_v<T> ? &CopyImpl<T> : nullptr),
            &MoveImpl<T>,
            &InvokeImpl<T>
        };

        // Placement new into storage
        new (&storage) T(AltinaEngine::Move(f));
        vtable = &vt;
    }

    VTable const* vtable;
    StorageT storage;
};

} // namespace
