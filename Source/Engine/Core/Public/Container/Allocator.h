#ifndef ALTINAENGINE_CORE_PUBLIC_CONTAINER_ALLOCATOR_H
#define ALTINAENGINE_CORE_PUBLIC_CONTAINER_ALLOCATOR_H

#include "../Base/CoreAPI.h"

namespace AltinaEngine::Core::Container
{

    /**
     * TAllocator<T>
     * Minimal allocator intended for use in engine containers. Stateless.
     */
    template <typename T> struct AE_CORE_API TAllocator
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
            void* p = ::operator new(static_cast<size_type>(n) * sizeof(value_type));
            return static_cast<pointer>(p);
        }

        inline pointer Allocate(size_type n, const_pointer /*hint*/) { return Allocate(n); }

        inline void    Deallocate(pointer p, size_type /*n*/) noexcept { ::operator delete(static_cast<void*>(p)); }

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

} // namespace AltinaEngine::Core::Container

#endif // ALTINAENGINE_CORE_PUBLIC_CONTAINER_ALLOCATOR_H
