#pragma once

#include "Allocator.h"
#include "Atomic.h"
#include "../Types/Traits.h"

namespace AltinaEngine::Core::Container
{
    template <typename T, typename D = TDefaultDeleter<T>>
    class TOwner
    {
    public:
        using pointer = T*;
        using element_type = T;
        using deleter_type = D;

        constexpr TOwner() noexcept : Ptr(nullptr) {}
        constexpr TOwner(decltype(nullptr)) noexcept : Ptr(nullptr) {}
        
        explicit TOwner(pointer p) noexcept : Ptr(p) {}
        
        TOwner(pointer p, const D& d) noexcept : Ptr(p), Deleter(d) {}
        TOwner(pointer p, D&& d) noexcept : Ptr(p), Deleter(AltinaEngine::Move(d)) {}

        TOwner(TOwner&& Other) noexcept
            : Ptr(Other.release()), Deleter(AltinaEngine::Forward<D>(Other.get_deleter())) {}

        TOwner& operator=(TOwner&& Other) noexcept
        {
            if (this != &Other)
            {
                reset(Other.release());
                Deleter = AltinaEngine::Move(Other.get_deleter());
            }
            return *this;
        }

        TOwner(const TOwner&) = delete;
        TOwner& operator=(const TOwner&) = delete;

        ~TOwner()
        {
            if (Ptr)
            {
                Deleter(Ptr);
            }
        }

        pointer release() noexcept
        {
            pointer p = Ptr;
            Ptr = nullptr;
            return p;
        }

        void reset(pointer p = nullptr) noexcept
        {
            pointer old = Ptr;
            Ptr = p;
            if (old)
            {
                Deleter(old);
            }
        }

        void swap(TOwner& Other) noexcept
        {
            pointer tmpPtr = Ptr;
            Ptr = Other.Ptr;
            Other.Ptr = tmpPtr;
            
            D tmpDeleter = AltinaEngine::Move(Deleter);
            Deleter = AltinaEngine::Move(Other.Deleter);
            Other.Deleter = AltinaEngine::Move(tmpDeleter);
        }

        pointer get() const noexcept { return Ptr; }
        D& get_deleter() noexcept { return Deleter; }
        const D& get_deleter() const noexcept { return Deleter; }
        explicit operator bool() const noexcept { return Ptr != nullptr; }

        typename TRemoveReference<T>::Type& operator*() const { return *Ptr; }
        pointer operator->() const noexcept { return Ptr; }

    private:
        pointer Ptr;
        D Deleter;
    };

    template <typename T, typename D>
    class TOwner<T[], D>
    {
    public:
        using pointer = T*;
        using element_type = T;
        using deleter_type = D;

        constexpr TOwner() noexcept : Ptr(nullptr) {}
        constexpr TOwner(decltype(nullptr)) noexcept : Ptr(nullptr) {}
        
        explicit TOwner(pointer p) noexcept : Ptr(p) {}
        
        TOwner(pointer p, const D& d) noexcept : Ptr(p), Deleter(d) {}
        TOwner(pointer p, D&& d) noexcept : Ptr(p), Deleter(AltinaEngine::Move(d)) {}

        TOwner(TOwner&& Other) noexcept
            : Ptr(Other.release()), Deleter(AltinaEngine::Forward<D>(Other.get_deleter())) {}

        TOwner& operator=(TOwner&& Other) noexcept
        {
            if (this != &Other)
            {
                reset(Other.release());
                Deleter = AltinaEngine::Move(Other.get_deleter());
            }
            return *this;
        }

        TOwner(const TOwner&) = delete;
        TOwner& operator=(const TOwner&) = delete;

        ~TOwner()
        {
            if (Ptr)
            {
                Deleter(Ptr);
            }
        }

        pointer release() noexcept
        {
            pointer p = Ptr;
            Ptr = nullptr;
            return p;
        }

        void reset(pointer p = nullptr) noexcept
        {
            pointer old = Ptr;
            Ptr = p;
            if (old)
            {
                Deleter(old);
            }
        }

        pointer get() const noexcept { return Ptr; }
        D& get_deleter() noexcept { return Deleter; }
        const D& get_deleter() const noexcept { return Deleter; }
        explicit operator bool() const noexcept { return Ptr != nullptr; }

        T& operator[](AltinaEngine::usize i) const { return Ptr[i]; }

    private:
        pointer Ptr;
        D Deleter;
    };

    template <typename T, typename... Args>
    TOwner<T> MakeUnique(Args&&... args)
    {
        if constexpr (kSmartPtrUseManagedAllocator)
        {
            TAllocator<T> Allocator;
            T* ptr = TAllocatorTraits<TAllocator<T>>::Allocate(Allocator, 1);
            TAllocatorTraits<TAllocator<T>>::Construct(Allocator, ptr, AltinaEngine::Forward<Args>(args)...);
            return TOwner<T>(ptr);
        }
        else
        {
            return TOwner<T>(new T(AltinaEngine::Forward<Args>(args)...));
        }
    }

    template <typename T, typename Alloc>
    struct TAllocatorDeleter
    {
        Alloc allocator;

        TAllocatorDeleter(const Alloc& a) : allocator(a) {}

        void operator()(T* ptr)
        {
            if (ptr)
            {
                TAllocatorTraits<Alloc>::Destroy(allocator, ptr);
                TAllocatorTraits<Alloc>::Deallocate(allocator, ptr, 1);
            }
        }
    };

    template <typename T, typename Alloc, typename... Args>
    TOwner<T, TAllocatorDeleter<T, Alloc>> AllocateUnique(Alloc& alloc, Args&&... args)
    {
        T* ptr = TAllocatorTraits<Alloc>::Allocate(alloc, 1);
        TAllocatorTraits<Alloc>::Construct(alloc, ptr, AltinaEngine::Forward<Args>(args)...);
        return TOwner<T, TAllocatorDeleter<T, Alloc>>(ptr, TAllocatorDeleter<T, Alloc>(alloc));
    }

    class FSharedControlBlock
    {
    public:
        FSharedControlBlock() noexcept : mRefCount(1) {}
        virtual ~FSharedControlBlock() = default;

        void AddRef() noexcept
        {
            mRefCount.FetchAdd(static_cast<AltinaEngine::usize>(1), EMemoryOrder::AcquireRelease);
        }

        bool ReleaseRef() noexcept
        {
            return mRefCount.FetchSub(static_cast<AltinaEngine::usize>(1), EMemoryOrder::AcquireRelease) == 1;
        }

        AltinaEngine::usize GetRefCount() const noexcept
        {
            return mRefCount.Load(EMemoryOrder::Acquire);
        }

        virtual void DestroyManaged() noexcept = 0;
        virtual void DestroySelf() noexcept = 0;

    private:
        TAtomic<AltinaEngine::usize> mRefCount;
    };

    template <typename T, typename Deleter>
    class TSharedControlBlock final : public FSharedControlBlock
    {
    public:
        TSharedControlBlock(T* ptr, Deleter deleter)
            : mPointer(ptr)
            , mDeleter(AltinaEngine::Move(deleter))
        {
        }

        void DestroyManaged() noexcept override
        {
            if (mPointer)
            {
                mDeleter(mPointer);
                mPointer = nullptr;
            }
        }

        void DestroySelf() noexcept override
        {
            delete this;
        }

    private:
        T*        mPointer;
        Deleter   mDeleter;
    };

    template <typename T>
    class TShared
    {
    public:
        using element_type = T;
        using pointer = element_type*;

        constexpr TShared() noexcept : Ptr(nullptr), Control(nullptr) {}

        TShared(const TShared& Other) noexcept
            : Ptr(Other.Ptr)
            , Control(Other.Control)
        {
            if (Control)
            {
                Control->AddRef();
            }
        }

        TShared(TShared&& Other) noexcept
            : Ptr(Other.Ptr)
            , Control(Other.Control)
        {
            Other.Ptr = nullptr;
            Other.Control = nullptr;
        }

        explicit TShared(pointer InPtr)
            : TShared(InPtr, TDefaultDeleter<element_type>{})
        {
        }

        template <typename D>
        explicit TShared(pointer InPtr, D&& InDeleter)
            : Ptr(InPtr)
            , Control(nullptr)
        {
            if (Ptr)
            {
                InitializeControlBlock(AltinaEngine::Forward<D>(InDeleter));
            }
        }

        ~TShared()
        {
            Release();
        }

        TShared& operator=(const TShared& Other)
        {
            if (this != &Other)
            {
                Release();
                Ptr = Other.Ptr;
                Control = Other.Control;
                if (Control)
                {
                    Control->AddRef();
                }
            }
            return *this;
        }

        TShared& operator=(TShared&& Other) noexcept
        {
            if (this != &Other)
            {
                Release();
                Ptr = Other.Ptr;
                Control = Other.Control;
                Other.Ptr = nullptr;
                Other.Control = nullptr;
            }
            return *this;
        }

        void Reset() noexcept
        {
            Release();
        }

        void Reset(pointer InPtr)
        {
            Reset(InPtr, TDefaultDeleter<element_type>{});
        }

        template <typename D>
        void Reset(pointer InPtr, D&& InDeleter)
        {
            Release();
            Ptr = InPtr;
            Control = nullptr;
            if (Ptr)
            {
                InitializeControlBlock(AltinaEngine::Forward<D>(InDeleter));
            }
        }

        void Swap(TShared& Other) noexcept
        {
            pointer tmpPtr = Ptr;
            Ptr = Other.Ptr;
            Other.Ptr = tmpPtr;

            FSharedControlBlock* tmpControl = Control;
            Control = Other.Control;
            Other.Control = tmpControl;
        }

        pointer Get() const noexcept { return Ptr; }
        element_type& operator*() const { return *Ptr; }
        pointer operator->() const noexcept { return Ptr; }
        explicit operator bool() const noexcept { return Ptr != nullptr; }

        AltinaEngine::usize UseCount() const noexcept
        {
            return Control ? Control->GetRefCount() : 0U;
        }

    private:
        template <typename D>
        using TDecayDeleter = typename AltinaEngine::TDecay<D>::Type;

        template <typename D>
        void InitializeControlBlock(D&& InDeleter)
        {
            using StorageType = TDecayDeleter<D>;
            using ControlType = TSharedControlBlock<element_type, StorageType>;

            StorageType SafeDeleter(AltinaEngine::Forward<D>(InDeleter));
            try
            {
                Control = new ControlType(Ptr, SafeDeleter);
            }
            catch (...)
            {
                SafeDeleter(Ptr);
                Ptr = nullptr;
                Control = nullptr;
                throw;
            }
        }

        void Release() noexcept
        {
            if (Control)
            {
                if (Control->ReleaseRef())
                {
                    Control->DestroyManaged();
                    Control->DestroySelf();
                }
            }
            Ptr = nullptr;
            Control = nullptr;
        }

        pointer             Ptr;
        FSharedControlBlock* Control;
    };

    template <typename T, typename... Args>
    TShared<T> MakeShared(Args&&... args)
    {
        if constexpr (kSmartPtrUseManagedAllocator)
        {
            TAllocator<T> Allocator;
            T* ptr = TAllocatorTraits<TAllocator<T>>::Allocate(Allocator, 1);
            try
            {
                TAllocatorTraits<TAllocator<T>>::Construct(
                    Allocator, ptr, AltinaEngine::Forward<Args>(args)...);
            }
            catch (...)
            {
                TAllocatorTraits<TAllocator<T>>::Deallocate(Allocator, ptr, 1);
                throw;
            }

            TOwner<T, TAllocatorDeleter<T, TAllocator<T>>> Owner(ptr, TAllocatorDeleter<T, TAllocator<T>>(Allocator));
            TShared<T> Result(Owner.get(), Owner.get_deleter());
            Owner.release();
            return Result;
        }
        else
        {
            TOwner<T> Owner(new T(AltinaEngine::Forward<Args>(args)...));
            TShared<T> Result(Owner.get(), Owner.get_deleter());
            Owner.release();
            return Result;
        }
    }

    template <typename T, typename Alloc, typename... Args>
    TShared<T> AllocateShared(Alloc& alloc, Args&&... args)
    {
        T* ptr = TAllocatorTraits<Alloc>::Allocate(alloc, 1);
        try
        {
            TAllocatorTraits<Alloc>::Construct(alloc, ptr, AltinaEngine::Forward<Args>(args)...);
        }
        catch (...)
        {
            TAllocatorTraits<Alloc>::Deallocate(alloc, ptr, 1);
            throw;
        }

        TOwner<T, TAllocatorDeleter<T, Alloc>> Owner(ptr, TAllocatorDeleter<T, Alloc>(alloc));
        TShared<T> Result(Owner.get(), Owner.get_deleter());
        Owner.release();
        return Result;
    }
}
