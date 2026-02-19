#ifndef ALTINAENGINE_CORE_PUBLIC_CONTAINER_ALLOCATOR_H
#define ALTINAENGINE_CORE_PUBLIC_CONTAINER_ALLOCATOR_H

#include "../Platform/Generic/GenericPlatformDecl.h"
#include "../Types/Traits.h"
#include "ContainerConfig.h"

using AltinaEngine::Forward;
namespace AltinaEngine::Core::Container {

    /**
     * TAllocator<T>
     * Minimal allocator intended for use in engine containers. Stateless.
     */
    template <typename T> struct TAllocator {
        using TValueType      = T;
        using TPointer        = TValueType*;
        using TConstPointer   = const TValueType*;
        using TReference      = TValueType&;
        using TConstReference = const TValueType&;
        using TSizeType       = unsigned long long;

        constexpr TAllocator() noexcept = default;

        template <typename U> constexpr TAllocator(const TAllocator<U>&) noexcept {}

        template <typename U> struct Rebind {
            using TOther = TAllocator<U>;
        };

        inline auto Allocate(const TSizeType n) -> TPointer {
            if (n == 0) {
                return nullptr;
            }
            using AltinaEngine::Core::Platform::FMemoryAllocator;
            using AltinaEngine::Core::Platform::GetGlobalMemoryAllocator;

            FMemoryAllocator* allocator = GetGlobalMemoryAllocator();
            void*             p         = allocator->MemoryAllocate(
                static_cast<usize>(n) * sizeof(TValueType), alignof(TValueType));
            return static_cast<TPointer>(p);
        }

        inline auto Allocate(const TSizeType n, TConstPointer /*hint*/) -> TPointer {
            return Allocate(n);
        }

        inline void Deallocate(TPointer p, TSizeType /*n*/) noexcept {
            if (!p) {
                return;
            }

            using Platform::FMemoryAllocator;
            using Platform::GetGlobalMemoryAllocator;

            FMemoryAllocator* allocator = GetGlobalMemoryAllocator();
            allocator->MemoryFree(static_cast<void*>(p));
        }

        template <typename... Args> void Construct(TPointer p, Args&&... args) {
            ::new (static_cast<void*>(p)) TValueType(Forward<Args>(args)...);
        }

        void Destroy(TPointer p) noexcept {
            if (p) {
                p->~TValueType();
            }
        }

        [[nodiscard]] constexpr auto MaxSize() const noexcept -> TSizeType {
            return ~static_cast<TSizeType>(0) / sizeof(TValueType);
        }

        constexpr auto operator==(const TAllocator&) const noexcept -> bool { return true; }
        constexpr auto operator!=(const TAllocator&) const noexcept -> bool { return false; }
    };

    template <typename Alloc> struct TAllocatorTraits {
        using TAllocatorType = Alloc;
        using TValueType     = Alloc::TValueType;
        using TPointer       = Alloc::TPointer;
        using TConstPointer  = Alloc::TConstPointer;
        using TSizeType      = Alloc::TSizeType;

        static auto Allocate(Alloc& a, TSizeType n) -> TPointer { return a.Allocate(n); }

        static void Deallocate(Alloc& a, TPointer p, TSizeType n) { a.Deallocate(p, n); }

        template <typename... Args> static void Construct(Alloc& a, TPointer p, Args&&... args) {
            a.Construct(p, Forward<Args>(args)...);
        }

        static void Destroy(Alloc& a, TPointer p) { a.Destroy(p); }
    };

    template <typename T> struct TDefaultDeleter {
        constexpr TDefaultDeleter() noexcept = default;

        void operator()(T* ptr) const {
            if (ptr) {
                if constexpr (kSmartPtrUseManagedAllocator) {
                    TAllocator<T> allocator;
                    TAllocatorTraits<TAllocator<T>>::Destroy(allocator, ptr);
                    TAllocatorTraits<TAllocator<T>>::Deallocate(allocator, ptr, 1);
                } else {
                    delete ptr; // NOLINT
                }
            }
        }
    };

    template <typename T>
    struct TDefaultDeleter<T[]> // NOLINT
    {
        constexpr TDefaultDeleter() noexcept = default;

        void operator()(T* ptr) const {
            delete[] ptr; // NOLINT
        }
    };

} // namespace AltinaEngine::Core::Container

#endif // ALTINAENGINE_CORE_PUBLIC_CONTAINER_ALLOCATOR_H
