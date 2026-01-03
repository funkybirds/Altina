#pragma once

#include "Allocator.h"
#include "../Threading/Atomic.h"
#include "../Types/Traits.h"

using AltinaEngine::Core::Threading::EMemoryOrder;
using AltinaEngine::Core::Threading::TAtomic;

namespace AltinaEngine::Core::Container {

    template <typename T, typename D = TDefaultDeleter<T>> class TOwner {
    public:
        using TPointer     = T*;
        using TElementType = T;
        using TDeleterType = D;

        constexpr TOwner() noexcept : Ptr(nullptr) {}
        constexpr TOwner(decltype(nullptr)) noexcept : Ptr(nullptr) {}

        explicit TOwner(TPointer p) noexcept : Ptr(p) {}

        TOwner(TPointer p, const D& d) noexcept : Ptr(p), Deleter(d) {}
        TOwner(TPointer p, D&& d) noexcept : Ptr(p), Deleter(AltinaEngine::Move(d)) {}

        TOwner(TOwner&& Other) noexcept
            : Ptr(Other.release()), Deleter(AltinaEngine::Forward<D>(Other.get_deleter())) {}

        TOwner& operator=(TOwner&& Other) noexcept {
            if (this != &Other) {
                reset(Other.release());
                Deleter = AltinaEngine::Move(Other.get_deleter());
            }
            return *this;
        }

        TOwner(const TOwner&)            = delete;
        TOwner& operator=(const TOwner&) = delete;

        ~TOwner() {
            if (Ptr) {
                Deleter(Ptr);
            }
        }

        TPointer release() noexcept {
            TPointer p = Ptr;
            Ptr        = nullptr;
            return p;
        }

        void reset(TPointer p = nullptr) noexcept {
            TPointer old = Ptr;
            Ptr          = p;
            if (old) {
                Deleter(old);
            }
        }

        void swap(TOwner& Other) noexcept {
            TPointer tmpPtr = Ptr;
            Ptr             = Other.Ptr;
            Other.Ptr       = tmpPtr;

            D tmpDeleter  = AltinaEngine::Move(Deleter);
            Deleter       = AltinaEngine::Move(Other.Deleter);
            Other.Deleter = AltinaEngine::Move(tmpDeleter);
        }

        TPointer                    get() const noexcept { return Ptr; }
        D&                          get_deleter() noexcept { return Deleter; }
        const D&                    get_deleter() const noexcept { return Deleter; }
        explicit                    operator bool() const noexcept { return Ptr != nullptr; }

        TRemoveReference<T>::TType& operator*() const { return *Ptr; }
        TPointer                    operator->() const noexcept { return Ptr; }

    private:
        TPointer Ptr;
        D        Deleter;
    };

    template <typename T, typename D> class TOwner<T[], D> {
    public:
        using pointer      = T*;
        using element_type = T;
        using deleter_type = D;

        constexpr TOwner() noexcept : Ptr(nullptr) {}
        constexpr TOwner(decltype(nullptr)) noexcept : Ptr(nullptr) {}

        explicit TOwner(pointer p) noexcept : Ptr(p) {}

        TOwner(pointer p, const D& d) noexcept : Ptr(p), Deleter(d) {}
        TOwner(pointer p, D&& d) noexcept : Ptr(p), Deleter(AltinaEngine::Move(d)) {}

        TOwner(TOwner&& Other) noexcept
            : Ptr(Other.release()), Deleter(AltinaEngine::Forward<D>(Other.get_deleter())) {}

        TOwner& operator=(TOwner&& Other) noexcept {
            if (this != &Other) {
                reset(Other.release());
                Deleter = AltinaEngine::Move(Other.get_deleter());
            }
            return *this;
        }

        TOwner(const TOwner&)            = delete;
        TOwner& operator=(const TOwner&) = delete;

        ~TOwner() {
            if (Ptr) {
                Deleter(Ptr);
            }
        }

        pointer release() noexcept {
            pointer p = Ptr;
            Ptr       = nullptr;
            return p;
        }

        void reset(pointer p = nullptr) noexcept {
            pointer old = Ptr;
            Ptr         = p;
            if (old) {
                Deleter(old);
            }
        }

        pointer  get() const noexcept { return Ptr; }
        D&       get_deleter() noexcept { return Deleter; }
        const D& get_deleter() const noexcept { return Deleter; }
        explicit operator bool() const noexcept { return Ptr != nullptr; }

        T&       operator[](usize i) const { return Ptr[i]; }

    private:
        pointer Ptr;
        D       Deleter;
    };

    template <typename T, typename... Args> TOwner<T> MakeUnique(Args&&... args) {
        if constexpr (kSmartPtrUseManagedAllocator) {
            TAllocator<T> Allocator;
            T*            ptr = TAllocatorTraits<TAllocator<T>>::Allocate(Allocator, 1);
            TAllocatorTraits<TAllocator<T>>::Construct(
                Allocator, ptr, AltinaEngine::Forward<Args>(args)...);
            return TOwner<T>(ptr);
        } else {
            return TOwner<T>(new T(AltinaEngine::Forward<Args>(args)...));
        }
    }

    template <typename T, typename Alloc> struct TAllocatorDeleter {
        Alloc allocator;

        TAllocatorDeleter(const Alloc& a) : allocator(a) {}

        void operator()(T* ptr) {
            if (ptr) {
                TAllocatorTraits<Alloc>::Destroy(allocator, ptr);
                TAllocatorTraits<Alloc>::Deallocate(allocator, ptr, 1);
            }
        }
    };

    template <typename T, typename Alloc, typename... Args>
    TOwner<T, TAllocatorDeleter<T, Alloc>> AllocateUnique(Alloc& alloc, Args&&... args) {
        T* ptr = TAllocatorTraits<Alloc>::Allocate(alloc, 1);
        TAllocatorTraits<Alloc>::Construct(alloc, ptr, AltinaEngine::Forward<Args>(args)...);
        return TOwner<T, TAllocatorDeleter<T, Alloc>>(ptr, TAllocatorDeleter<T, Alloc>(alloc));
    }

    class FSharedControlBlock {
    public:
        FSharedControlBlock() noexcept : mRefCount(1) {}
        virtual ~FSharedControlBlock() = default;

        void AddRef() noexcept {
            mRefCount.FetchAdd(static_cast<usize>(1), EMemoryOrder::AcquireRelease);
        }

        bool ReleaseRef() noexcept {
            return mRefCount.FetchSub(static_cast<usize>(1), EMemoryOrder::AcquireRelease) == 1;
        }

        usize        GetRefCount() const noexcept { return mRefCount.Load(EMemoryOrder::Acquire); }

        virtual void DestroyManaged() noexcept = 0;
        virtual void DestroySelf() noexcept    = 0;

    private:
        TAtomic<usize> mRefCount;
    };

    template <typename T, typename Deleter>
    class TSharedControlBlock final : public FSharedControlBlock {
    public:
        TSharedControlBlock(T* ptr, Deleter deleter)
            : mPointer(ptr), mDeleter(AltinaEngine::Move(deleter)) {}

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
        using element_type = T;
        using TPointer     = element_type*;

        constexpr TShared() noexcept : mPtr(nullptr), mControl(nullptr) {}

        TShared(const TShared& Other) noexcept : mPtr(Other.mPtr), mControl(Other.mControl) {
            if (mControl) {
                mControl->AddRef();
            }
        }

        TShared(TShared&& Other) noexcept : mPtr(Other.mPtr), mControl(Other.mControl) {
            Other.mPtr     = nullptr;
            Other.mControl = nullptr;
        }

        explicit TShared(TPointer InPtr) : TShared(InPtr, TDefaultDeleter<element_type>{}) {}

        template <typename D>
        explicit TShared(TPointer InPtr, D&& InDeleter) : mPtr(InPtr), mControl(nullptr) {
            if (mPtr) {
                InitializeControlBlock(AltinaEngine::Forward<D>(InDeleter));
            }
        }

        ~TShared() { Release(); }

        TShared& operator=(const TShared& Other) {
            if (this != &Other) {
                Release();
                mPtr     = Other.mPtr;
                mControl = Other.mControl;
                if (mControl) {
                    mControl->AddRef();
                }
            }
            return *this;
        }

        TShared& operator=(TShared&& Other) noexcept {
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

        void Reset(TPointer InPtr) { Reset(InPtr, TDefaultDeleter<element_type>{}); }

        template <typename D> void Reset(TPointer InPtr, D&& InDeleter) {
            Release();
            mPtr     = InPtr;
            mControl = nullptr;
            if (mPtr) {
                InitializeControlBlock(AltinaEngine::Forward<D>(InDeleter));
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

        TPointer      Get() const noexcept { return mPtr; }
        element_type& operator*() const { return *mPtr; }
        TPointer      operator->() const noexcept { return mPtr; }
        explicit      operator bool() const noexcept { return mPtr != nullptr; }

        auto UseCount() const noexcept -> usize { return mControl ? mControl->GetRefCount() : 0U; }

    private:
        template <typename D> using TDecayDeleter = TDecay<D>::TType;

        template <typename D> void InitializeControlBlock(D&& InDeleter) {
            using TStorageType = TDecayDeleter<D>;
            using TControlType = TSharedControlBlock<element_type, TStorageType>;

            TStorageType safeDeleter(AltinaEngine::Forward<D>(InDeleter));
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
            if (mControl) {
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
                TAllocatorTraits<TAllocator<T>>::Construct(
                    allocator, ptr, AltinaEngine::Forward<Args>(args)...);
            } catch (...) {
                TAllocatorTraits<TAllocator<T>>::Deallocate(allocator, ptr, 1);
                throw;
            }

            TOwner<T, TAllocatorDeleter<T, TAllocator<T>>> owner(
                ptr, TAllocatorDeleter<T, TAllocator<T>>(allocator));
            TShared<T> result(owner.get(), owner.get_deleter());
            owner.release();
            return result;
        } else {
            TOwner<T>  Owner(new T(AltinaEngine::Forward<Args>(args)...));
            TShared<T> Result(Owner.get(), Owner.get_deleter());
            Owner.release();
            return Result;
        }
    }

    template <typename T, typename Alloc, typename... Args>
    TShared<T> AllocateShared(Alloc& alloc, Args&&... args) {
        T* ptr = TAllocatorTraits<Alloc>::Allocate(alloc, 1);
        try {
            TAllocatorTraits<Alloc>::Construct(alloc, ptr, AltinaEngine::Forward<Args>(args)...);
        } catch (...) {
            TAllocatorTraits<Alloc>::Deallocate(alloc, ptr, 1);
            throw;
        }

        TOwner<T, TAllocatorDeleter<T, Alloc>> owner(ptr, TAllocatorDeleter<T, Alloc>(alloc));
        TShared<T>                             result(owner.get(), owner.get_deleter());
        owner.release();
        return result;
    }
} // namespace AltinaEngine::Core::Container
