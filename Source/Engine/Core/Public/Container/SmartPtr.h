#pragma once

#include "Allocator.h"
#include "../Threading/Atomic.h"
#include "../Types/Traits.h"

using AltinaEngine::CClassBaseOf;
using AltinaEngine::Forward;
using AltinaEngine::Move;

using AltinaEngine::Core::Threading::EMemoryOrder;
using AltinaEngine::Core::Threading::TAtomic;

namespace AltinaEngine::Core::Container {

    template <typename T, typename D = TDefaultDeleter<T>> class TOwner {
    public:
        using TPointer     = T*;
        using TElementType = T;
        using TDeleterType = D;

        constexpr TOwner() noexcept : mPtr(nullptr) {}
        constexpr TOwner(decltype(nullptr)) noexcept : mPtr(nullptr) {}

        explicit TOwner(TPointer p) noexcept : mPtr(p) {}

        TOwner(TPointer p, const D& d) noexcept : mPtr(p), mDeleter(d) {}
        TOwner(TPointer p, D&& d) noexcept : mPtr(p), mDeleter(Move(d)) {}

        TOwner(TOwner&& Other) noexcept
            : mPtr(Other.Release()), mDeleter(Forward<D>(Other.GetDeleter())) {}

        auto operator=(TOwner&& Other) noexcept -> TOwner& {
            if (this != &Other) {
                Reset(Other.Release());
                mDeleter = Move(Other.GetDeleter());
            }
            return *this;
        }

        TOwner(const TOwner&)                    = delete;
        auto operator=(const TOwner&) -> TOwner& = delete;

        ~TOwner() {
            if (mPtr) {
                mDeleter(mPtr);
            }
        }

        auto Release() noexcept -> TPointer {
            TPointer p = mPtr;
            mPtr       = nullptr;
            return p;
        }

        void Reset(TPointer p = nullptr) noexcept {
            TPointer old = mPtr;
            mPtr         = p;
            if (old) {
                mDeleter(old);
            }
        }

        void Swap(TOwner& Other) noexcept {
            TPointer tmpPtr = mPtr;
            mPtr            = Other.mPtr;
            Other.mPtr      = tmpPtr;

            D tmpDeleter   = Move(mDeleter);
            mDeleter       = Move(Other.mDeleter);
            Other.mDeleter = Move(tmpDeleter);
        }

        auto     Get() const noexcept -> TPointer { return mPtr; }
        auto     GetDeleter() noexcept -> D& { return mDeleter; }
        auto     GetDeleter() const noexcept -> const D& { return mDeleter; }
        explicit operator bool() const noexcept { return mPtr != nullptr; }

        auto     operator*() const -> TRemoveReference<T>::TType& { return *mPtr; }
        auto     operator->() const noexcept -> TPointer { return mPtr; }

    private:
        TPointer mPtr;
        D        mDeleter;
    };

    template <typename T, typename D> class TOwner<T[], D> {
    public:
        using TPointer     = T*;
        using TElementType = T;
        using TDeleterType = D;

        constexpr TOwner() noexcept : mPtr(nullptr) {}
        constexpr TOwner(decltype(nullptr)) noexcept : mPtr(nullptr) {}

        explicit TOwner(TPointer p) noexcept : mPtr(p) {}

        TOwner(TPointer p, const D& d) noexcept : mPtr(p), mDeleter(d) {}
        TOwner(TPointer p, D&& d) noexcept : mPtr(p), mDeleter(Move(d)) {}

        TOwner(TOwner&& Other) noexcept
            : mPtr(Other.Release()), mDeleter(Forward<D>(Other.GetDeleter())) {}

        auto operator=(TOwner&& Other) noexcept -> TOwner& {
            if (this != &Other) {
                Reset(Other.Release());
                mDeleter = Move(Other.GetDeleter());
            }
            return *this;
        }

        TOwner(const TOwner&)                    = delete;
        auto operator=(const TOwner&) -> TOwner& = delete;

        ~TOwner() {
            if (mPtr) {
                mDeleter(mPtr);
            }
        }

        auto Release() noexcept -> TPointer {
            TPointer p = mPtr;
            mPtr       = nullptr;
            return p;
        }

        void Reset(TPointer p = nullptr) noexcept {
            TPointer old = mPtr;
            mPtr         = p;
            if (old) {
                mDeleter(old);
            }
        }

        auto     Get() const noexcept -> TPointer { return mPtr; }
        auto     GetDeleter() noexcept -> D& { return mDeleter; }
        auto     GetDeleter() const noexcept -> const D& { return mDeleter; }
        explicit operator bool() const noexcept { return mPtr != nullptr; }

        auto     operator[](usize i) const -> T& { return mPtr[i]; }

    private:
        TPointer mPtr;
        D        mDeleter;
    };

    template <typename T, typename... Args> auto MakeUnique(Args&&... args) -> TOwner<T> {
        if constexpr (kSmartPtrUseManagedAllocator) {
            TAllocator<T> allocator;
            T*            ptr = TAllocatorTraits<TAllocator<T>>::Allocate(allocator, 1);
            TAllocatorTraits<TAllocator<T>>::Construct(allocator, ptr, Forward<Args>(args)...);
            return TOwner<T>(ptr);
        } else {
            return TOwner<T>(new T(Forward<Args>(args)...));
        }
    }

    template <typename T> class TPolymorphicDeleter {
    public:
        using TDestroyFn = void (*)(T*);

        constexpr TPolymorphicDeleter() noexcept = default;
        explicit constexpr TPolymorphicDeleter(TDestroyFn fn) noexcept : mDestroy(fn) {}

        void operator()(T* ptr) const noexcept {
            if (ptr && mDestroy) {
                mDestroy(ptr);
            }
        }

    private:
        TDestroyFn mDestroy = nullptr;
    };

    template <typename TBase, typename TDerived>
        requires CClassBaseOf<TBase, TDerived>
    inline void DestroyPolymorphic(TBase* basePtr) noexcept {
        if (!basePtr) {
            return;
        }

        auto* derivedPtr = static_cast<TDerived*>(basePtr);
        if constexpr (kSmartPtrUseManagedAllocator) {
            TAllocator<TDerived> allocator;
            TAllocatorTraits<TAllocator<TDerived>>::Destroy(allocator, derivedPtr);
            TAllocatorTraits<TAllocator<TDerived>>::Deallocate(allocator, derivedPtr, 1);
        } else {
            delete derivedPtr; // NOLINT
        }
    }

    template <typename TBase, typename TDerived, typename... Args>
        requires CClassBaseOf<TBase, TDerived>
    auto MakeUniqueAs(Args&&... args) -> TOwner<TBase, TPolymorphicDeleter<TBase>> {
        TDerived* ptr = nullptr;
        if constexpr (kSmartPtrUseManagedAllocator) {
            TAllocator<TDerived> allocator;
            ptr = TAllocatorTraits<TAllocator<TDerived>>::Allocate(allocator, 1);
            if (ptr != nullptr) {
                try {
                    TAllocatorTraits<TAllocator<TDerived>>::Construct(
                        allocator, ptr, Forward<Args>(args)...);
                } catch (...) {
                    TAllocatorTraits<TAllocator<TDerived>>::Deallocate(allocator, ptr, 1);
                    throw;
                }
            }
        } else {
            ptr = new TDerived(Forward<Args>(args)...); // NOLINT
        }

        return TOwner<TBase, TPolymorphicDeleter<TBase>>(
            ptr, TPolymorphicDeleter<TBase>(&DestroyPolymorphic<TBase, TDerived>));
    }

    template <typename T, typename Alloc> struct TAllocatorDeleter {
        Alloc mAllocator;

        TAllocatorDeleter(const Alloc& a) : mAllocator(a) {}

        void operator()(T* ptr) {
            if (ptr) {
                TAllocatorTraits<Alloc>::Destroy(mAllocator, ptr);
                TAllocatorTraits<Alloc>::Deallocate(mAllocator, ptr, 1);
            }
        }
    };

    template <typename T, typename Alloc, typename... Args>
    auto AllocateUnique(Alloc& alloc, Args&&... args) -> TOwner<T, TAllocatorDeleter<T, Alloc>> {
        T* ptr = TAllocatorTraits<Alloc>::Allocate(alloc, 1);
        TAllocatorTraits<Alloc>::Construct(alloc, ptr, Forward<Args>(args)...);
        return TOwner<T, TAllocatorDeleter<T, Alloc>>(ptr, TAllocatorDeleter<T, Alloc>(alloc));
    }

    class FSharedControlBlock {
    public:
        FSharedControlBlock() noexcept : mRefCount(1) {}
        virtual ~FSharedControlBlock() = default;

        void AddRef() noexcept {
            mRefCount.FetchAdd(static_cast<usize>(1), EMemoryOrder::AcquireRelease);
        }

        auto ReleaseRef() noexcept -> bool {
            return mRefCount.FetchSub(static_cast<usize>(1), EMemoryOrder::AcquireRelease) == 1;
        }

        auto GetRefCount() const noexcept -> usize { return mRefCount.Load(EMemoryOrder::Acquire); }

        virtual void DestroyManaged() noexcept = 0;
        virtual void DestroySelf() noexcept    = 0;

    private:
        TAtomic<usize> mRefCount;
    };

    template <typename T, typename Deleter>
    class TSharedControlBlock final : public FSharedControlBlock {
    public:
        TSharedControlBlock(T* ptr, Deleter deleter) : mPointer(ptr), mDeleter(Move(deleter)) {}

        void DestroyManaged() noexcept override {
            if (mPointer) {
                mDeleter(mPointer);
                mPointer = nullptr;
            }
        }

        void DestroySelf() noexcept override { delete this; }

    private:
        T*      mPointer;
        Deleter mDeleter;
    };

    template <typename T> class TShared {
    public:
        using TElementType = T;
        using TPointer     = TElementType*;

        constexpr TShared() noexcept : mPtr(nullptr), mControl(nullptr) {}

        TShared(const TShared& Other) noexcept : mPtr(Other.mPtr), mControl(Other.mControl) {
            if (mControl != nullptr) {
                mControl->AddRef();
            }
        }

        TShared(TShared&& Other) noexcept : mPtr(Other.mPtr), mControl(Other.mControl) {
            Other.mPtr     = nullptr;
            Other.mControl = nullptr;
        }

        explicit TShared(TPointer InPtr) : TShared(InPtr, TDefaultDeleter<TElementType>{}) {}

        template <typename D>
        explicit TShared(TPointer InPtr, D&& InDeleter) : mPtr(InPtr), mControl(nullptr) {
            if (mPtr) {
                InitializeControlBlock(Forward<D>(InDeleter));
            }
        }

        ~TShared() { Release(); }

        auto operator=(const TShared& Other) -> TShared& {
            if (this != &Other) {
                Release();
                mPtr     = Other.mPtr;
                mControl = Other.mControl;
                if (mControl != nullptr) {
                    mControl->AddRef();
                }
            }
            return *this;
        }

        auto operator=(TShared&& Other) noexcept -> TShared& {
            if (this != &Other) {
                Release();
                mPtr           = Other.mPtr;
                mControl       = Other.mControl;
                Other.mPtr     = nullptr;
                Other.mControl = nullptr;
            }
            return *this;
        }

        void Reset() noexcept { Release(); }

        void Reset(TPointer InPtr) { Reset(InPtr, TDefaultDeleter<TElementType>{}); }

        template <typename D> void Reset(TPointer InPtr, D&& InDeleter) {
            Release();
            mPtr     = InPtr;
            mControl = nullptr;
            if (mPtr) {
                InitializeControlBlock(Forward<D>(InDeleter));
            }
        }

        void Swap(TShared& Other) noexcept {
            TPointer tmpPtr = mPtr;
            mPtr            = Other.mPtr;
            Other.mPtr      = tmpPtr;

            FSharedControlBlock* tmpControl = mControl;
            mControl                        = Other.mControl;
            Other.mControl                  = tmpControl;
        }

        auto               Get() const noexcept -> TPointer { return mPtr; }
        auto               operator*() const -> TElementType& { return *mPtr; }
        auto               operator->() const noexcept -> TPointer { return mPtr; }
        explicit           operator bool() const noexcept { return mPtr != nullptr; }

        [[nodiscard]] auto UseCount() const noexcept -> usize {
            return (mControl != nullptr) ? mControl->GetRefCount() : 0U;
        }

    private:
        template <typename D> using TDecayDeleter = TDecay<D>::TType;

        template <typename D> void InitializeControlBlock(D&& InDeleter) {
            using TStorageType = TDecayDeleter<D>;
            using TControlType = TSharedControlBlock<TElementType, TStorageType>;

            TStorageType safeDeleter(Forward<D>(InDeleter));
            try {
                mControl = new TControlType(mPtr, safeDeleter);
            } catch (...) {
                safeDeleter(mPtr);
                mPtr     = nullptr;
                mControl = nullptr;
                throw;
            }
        }

        void Release() noexcept {
            if (mControl != nullptr) {
                if (mControl->ReleaseRef()) {
                    mControl->DestroyManaged();
                    mControl->DestroySelf();
                }
            }
            mPtr     = nullptr;
            mControl = nullptr;
        }

        TPointer             mPtr;
        FSharedControlBlock* mControl;
    };

    template <typename T, typename... Args> auto MakeShared(Args&&... args) -> TShared<T> {
        if constexpr (kSmartPtrUseManagedAllocator) {
            TAllocator<T> allocator;
            T*            ptr = TAllocatorTraits<TAllocator<T>>::Allocate(allocator, 1);
            try {
                TAllocatorTraits<TAllocator<T>>::Construct(allocator, ptr, Forward<Args>(args)...);
            } catch (...) {
                TAllocatorTraits<TAllocator<T>>::Deallocate(allocator, ptr, 1);
                throw;
            }

            TOwner<T, TAllocatorDeleter<T, TAllocator<T>>> owner(
                ptr, TAllocatorDeleter<T, TAllocator<T>>(allocator));
            TShared<T> result(owner.Get(), owner.GetDeleter());
            owner.Release();
            return result;
        } else {
            TOwner<T>  Owner(new T(Forward<Args>(args)...));
            TShared<T> Result(Owner.Get(), Owner.GetDeleter());
            Owner.Release();
            return Result;
        }
    }

    template <typename T, typename Alloc, typename... Args>
    auto AllocateShared(Alloc& alloc, Args&&... args) -> TShared<T> {
        T* ptr = TAllocatorTraits<Alloc>::Allocate(alloc, 1);
        try {
            TAllocatorTraits<Alloc>::Construct(alloc, ptr, Forward<Args>(args)...);
        } catch (...) {
            TAllocatorTraits<Alloc>::Deallocate(alloc, ptr, 1);
            throw;
        }

        TOwner<T, TAllocatorDeleter<T, Alloc>> owner(ptr, TAllocatorDeleter<T, Alloc>(alloc));
        TShared<T>                             result(owner.Get(), owner.GetDeleter());
        owner.Release();
        return result;
    }
} // namespace AltinaEngine::Core::Container
