#ifndef ALTINAENGINE_CORE_PUBLIC_CONTAINER_ALLOCATOR_H
#define ALTINAENGINE_CORE_PUBLIC_CONTAINER_ALLOCATOR_H

#include "../Base/CoreAPI.h"
#include "../Platform/Generic/GenericPlatformDecl.h"
#include "../Types/Traits.h"
#include "ContainerConfig.h"

namespace AltinaEngine::Core::Container
{

    /**
     * TAllocator<T>
     * Minimal allocator intended for use in engine containers. Stateless.
     */
    template <typename T> struct TAllocator
    {
        using value_type      = T;
        using pointer         = value_type*;
        using const_pointer   = const value_type*;
        using reference       = value_type&;
        using const_reference = const value_type&;
        using size_type       = unsigned long long;

        constexpr TAllocator() noexcept = default;

        template <typename U> constexpr TAllocator(const TAllocator<U>&) noexcept {}

        template <typename U> struct rebind
        {
            using other = TAllocator<U>;
        };

        inline pointer Allocate(size_type n)
        {
            if (n == 0)
            {
                return nullptr;
            }
            using AltinaEngine::Core::Platform::FMemoryAllocator;
            using AltinaEngine::Core::Platform::GetGlobalMemoryAllocator;

            FMemoryAllocator* Allocator = GetGlobalMemoryAllocator();
            void* p = Allocator->MemoryAllocate(static_cast<usize>(n) * sizeof(value_type), alignof(value_type));
            return static_cast<pointer>(p);
        }

        inline pointer Allocate(size_type n, const_pointer /*hint*/) { return Allocate(n); }

        inline void    Deallocate(pointer p, size_type /*n*/) noexcept
        {
            if (!p)
            {
                return;
            }

            using AltinaEngine::Core::Platform::FMemoryAllocator;
            using AltinaEngine::Core::Platform::GetGlobalMemoryAllocator;

            FMemoryAllocator* Allocator = GetGlobalMemoryAllocator();
            Allocator->MemoryFree(static_cast<void*>(p));
        }

        template <typename... Args> inline void Construct(pointer p, Args&&... args)
        {
            ::new (static_cast<void*>(p)) value_type(static_cast<Args&&>(args)...);
        }

        inline void Destroy(pointer p) noexcept
        {
            if (p)
            {
                p->~value_type();
            }
        }

        constexpr size_type MaxSize() const noexcept
        {
            return static_cast<size_type>(~static_cast<size_type>(0)) / sizeof(value_type);
        }

        constexpr bool operator==(const TAllocator&) const noexcept { return true; }
        constexpr bool operator!=(const TAllocator&) const noexcept { return false; }
    };

    template <typename Alloc> struct TAllocatorTraits
    {
        using allocator_type = Alloc;
        using value_type     = typename Alloc::value_type;
        using pointer        = typename Alloc::pointer;
        using const_pointer  = typename Alloc::const_pointer;
        using size_type      = typename Alloc::size_type;

        static pointer                          Allocate(Alloc& a, size_type n) { return a.Allocate(n); }

        static void                             Deallocate(Alloc& a, pointer p, size_type n) { a.Deallocate(p, n); }

        template <typename... Args> static void Construct(Alloc& a, pointer p, Args&&... args)
        {
            a.Construct(p, AltinaEngine::Forward<Args>(args)...);
        }

        static void Destroy(Alloc& a, pointer p) { a.Destroy(p); }
    };

    template <typename T> struct TDefaultDeleter
    {
        constexpr TDefaultDeleter() noexcept = default;

        void operator()(T* ptr) const
        {
            if (ptr)
            {
                if constexpr (kSmartPtrUseManagedAllocator)
                {
                    TAllocator<T> Allocator;
                    TAllocatorTraits<TAllocator<T>>::Destroy(Allocator, ptr);
                    TAllocatorTraits<TAllocator<T>>::Deallocate(Allocator, ptr, 1);
                }
                else
                {
                    delete ptr;
                }
            }
        }
    };

    template <typename T> struct TDefaultDeleter<T[]>
    {
        constexpr TDefaultDeleter() noexcept = default;

        void operator()(T* ptr) const
        {
            if (ptr)
            {
                delete[] ptr;
            }
        }
    };

} // namespace AltinaEngine::Core::Container

#endif // ALTINAENGINE_CORE_PUBLIC_CONTAINER_ALLOCATOR_H
