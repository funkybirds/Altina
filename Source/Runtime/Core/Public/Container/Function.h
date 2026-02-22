#pragma once

// Minimal TFunction - a small std::function-like wrapper without using STL
// Features: type-erased callable, small-buffer optimization, copy/move, invoke

#include "../Types/Aliases.h"
#include "../Types/Traits.h"
#include "../Platform/Generic/GenericPlatformDecl.h"

using AltinaEngine::Forward;
using AltinaEngine::Move;

namespace AltinaEngine::Core::Container {

    template <typename> class TFunction;

    template <typename R, typename... Args> class TFunction<R(Args...)> {
    public:
        using TResultType = R;

        TFunction() noexcept : mVTable(nullptr) {}

        // Construct from callable (disable for TFunction itself)
        template <typename F>
            requires(!CSameAs<typename TDecay<F>::TType, TFunction>)
        TFunction(F&& f) noexcept {
            using TDecayF = typename TRemoveReference<F>::TType;
            Init<TDecayF>(Move(f));
        }

        TFunction(const TFunction& other) : mVTable(nullptr) {
            if (other.mVTable && other.mVTable->mCopy) {
                other.mVTable->mCopy(&other.mStorage, &mStorage);
                mVTable = other.mVTable;
            }
        }

        TFunction(TFunction&& other) noexcept {
            auto tmp      = other.mVTable;
            other.mVTable = nullptr;
            if (tmp) {
                tmp->mMove(&other.mStorage, &mStorage);
                mVTable = tmp;
            } else {
                mVTable = nullptr;
            }
            // Clear moved-from storage memory to avoid stale bits
            Platform::Generic::Memset(&other.mStorage, 0, sizeof(StorageT));
        }

        auto operator=(const TFunction& other) -> TFunction& {
            if (this == &other)
                return *this;
            Reset();
            if (other.mVTable && other.mVTable->mCopy) {
                other.mVTable->mCopy(&other.mStorage, &mStorage);
                mVTable = other.mVTable;
            }
            return *this;
        }

        auto operator=(TFunction&& other) noexcept -> TFunction& {
            if (this == &other)
                return *this;
            Reset();
            auto tmp      = other.mVTable;
            other.mVTable = nullptr;
            if (tmp) {
                tmp->mMove(&other.mStorage, &mStorage);
                mVTable = tmp;
            } else {
                mVTable = nullptr;
            }
            Platform::Generic::Memset(&other.mStorage, 0, sizeof(StorageT));
            return *this;
        }

        // Assignment from callable (disable for TFunction itself)
        template <typename F>
            requires(!CSameAs<typename TDecay<F>::TType, TFunction>)
        auto operator=(F&& f) noexcept -> TFunction& {
            Reset();
            using TDecayF = typename TRemoveReference<F>::TType;
            Init<TDecayF>(Move(f));
            return *this;
        }

        ~TFunction() { Reset(); }

        explicit operator bool() const noexcept { return mVTable != nullptr; }

        auto     operator()(Args... args) -> R {
            return mVTable->mInvoke(&mStorage, Forward<Args>(args)...);
        }

        void Reset() noexcept {
            if (mVTable)
                mVTable->mDestroy(&mStorage);
            mVTable = nullptr;
        }

    private:
        // Small buffer capacity (in bytes). Increased to accommodate larger
        // callable objects (closures capturing small smart pointers) so the
        // small-buffer optimization doesn't overflow for common use-cases.
        static constexpr usize kSmallSize  = 8 * sizeof(void*);
        static constexpr usize kSmallAlign = alignof(void*);

        struct StorageT {
            alignas(kSmallAlign) unsigned char mData[kSmallSize];
        };

        struct VTable {
            void (*mDestroy)(void* storage) noexcept;
            void (*mCopy)(const void* src, void* dst);
            void (*mMove)(void* src, void* dst) noexcept;
            R (*mInvoke)(void* storage, Args... args);
        };

        template <typename F> static void DestroyImpl(void* storage) noexcept {
            reinterpret_cast<F*>(storage)->~F();
        }

        template <typename F> static void CopyImpl(const void* src, void* dst) {
            if constexpr (CCopyConstructible<F>) {
                new (dst) F(*reinterpret_cast<const F*>(src));
            } else {
                // Should not be called when type is non-copyable; guard at vtable level.
                Platform::Generic::PlatformTerminate();
            }
        }

        template <typename F> static void MoveImpl(void* src, void* dst) noexcept {
            new (dst) F(Move(*reinterpret_cast<F*>(src)));
            reinterpret_cast<F*>(src)->~F();
        }

        template <typename F> static auto InvokeImpl(void* storage, Args... args) -> R {
            return (*reinterpret_cast<F*>(storage))(Forward<Args>(args)...);
        }

        // Heap-backed helpers: when callable is too large for small-buffer, store pointer to heap
        // object
        template <typename F> static void DestroyHeapImpl(void* storage) noexcept {
            F* ptr = *reinterpret_cast<F**>(storage);
            delete ptr;
        }

        template <typename F> static void CopyHeapImpl(const void* src, void* dst) {
            if constexpr (CCopyConstructible<F>) {
                F* srcPtr                   = *reinterpret_cast<F* const*>(src);
                *reinterpret_cast<F**>(dst) = new F(*srcPtr);
            } else {
                Platform::Generic::PlatformTerminate();
            }
        }

        template <typename F> static void MoveHeapImpl(void* src, void* dst) noexcept {
            *reinterpret_cast<F**>(dst) = *reinterpret_cast<F**>(src);
            *reinterpret_cast<F**>(src) = nullptr;
        }

        template <typename F> static auto InvokeHeapImpl(void* storage, Args... args) -> R {
            F* ptr = *reinterpret_cast<F**>(storage);
            return (*ptr)(Forward<Args>(args)...);
        }

        template <typename F> void Init(F&& f) noexcept {
            using T = typename TRemoveReference<F>::TType;

            if constexpr (sizeof(T) <= sizeof(StorageT)) {
                static const VTable vt = { &DestroyImpl<T>,
                    (CCopyConstructible<T> ? &CopyImpl<T> : nullptr), &MoveImpl<T>,
                    &InvokeImpl<T> };

                // Placement new into storage
                new (&mStorage) T(Move(f));
                mVTable = &vt;
            } else {
                static const VTable vt = { &DestroyHeapImpl<T>,
                    (CCopyConstructible<T> ? &CopyHeapImpl<T> : nullptr), &MoveHeapImpl<T>,
                    &InvokeHeapImpl<T> };

                // allocate on heap and store pointer in storage
                T*                  heapObj       = new T(Move(f));
                *reinterpret_cast<T**>(&mStorage) = heapObj;
                mVTable                           = &vt;
            }
        }

        VTable const* mVTable;
        StorageT      mStorage;
    };

} // namespace AltinaEngine::Core::Container
