#include "Platform/Generic/GenericPlatformDecl.h"

#include <algorithm>
#include <cstddef>
#include <cstdlib>

#if AE_PLATFORM_WIN
    #include <malloc.h>
#endif

namespace AltinaEngine::Core::Platform
{

    namespace
    {
        // Normalize alignment to a sensible power-of-two minimum.
        [[nodiscard]] constexpr usize NormalizeAlignment(usize Alignment) noexcept
        {
            if (Alignment == 0U)
            {
                return alignof(std::max_align_t);
            }

            if ((Alignment & (Alignment - 1U)) != 0U)
            {
                usize PowerOfTwo = 1U;
                while (PowerOfTwo < Alignment)
                {
                    PowerOfTwo <<= 1U;
                }
                Alignment = PowerOfTwo;
            }

            return std::max<usize>(Alignment, alignof(std::max_align_t));
        }
    } // namespace

    class FDefaultMemoryAllocator final : public FMemoryAllocator
    {
    public:
        void* MemoryAllocate(usize Size, usize Alignment) override
        {
            if (Size == 0U)
            {
                return nullptr;
            }

            const usize NormalizedAlignment = NormalizeAlignment(Alignment);

#if AE_PLATFORM_WIN
            return _aligned_malloc(Size, NormalizedAlignment);
#else
            if (NormalizedAlignment <= alignof(std::max_align_t))
            {
                return std::malloc(Size);
            }

            void* Result = nullptr;
            const int  Error  = posix_memalign(&Result, NormalizedAlignment, Size);
            return (Error == 0) ? Result : nullptr;
#endif
        }

        void* MemoryReallocate(void* Ptr, usize NewSize, usize Alignment) override
        {
            if (Ptr == nullptr)
            {
                return MemoryAllocate(NewSize, Alignment);
            }

            if (NewSize == 0U)
            {
                MemoryFree(Ptr);
                return nullptr;
            }

            const usize NormalizedAlignment = NormalizeAlignment(Alignment);

#if AE_PLATFORM_WIN
            return _aligned_realloc(Ptr, NewSize, NormalizedAlignment);
#else
            if (NormalizedAlignment <= alignof(std::max_align_t))
            {
                return std::realloc(Ptr, NewSize);
            }

            void* Result = nullptr;
            const int  Error  = posix_memalign(&Result, NormalizedAlignment, NewSize);

            if (Error != 0)
            {
                return nullptr;
            }

            // Without size information we cannot preserve the previous contents.
            std::free(Ptr);
            return Result;
#endif
        }

        void MemoryFree(void* Ptr) override
        {
#if AE_PLATFORM_WIN
            _aligned_free(Ptr);
#else
            std::free(Ptr);
#endif
        }
    };

    FMemoryAllocator* GetGlobalMemoryAllocator() noexcept
    {
        static FDefaultMemoryAllocator GDefaultAllocator;
        return &GDefaultAllocator;
    }

} // namespace AltinaEngine::Core::Platform
