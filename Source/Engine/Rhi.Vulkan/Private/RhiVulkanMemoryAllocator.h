#pragma once

#include "RhiVulkanInternal.h"
#include "Container/Vector.h"
#include "Container/String.h"
#include "Memory/BuddyAllocatorPolicy.h"
#include "Threading/Mutex.h"

namespace AltinaEngine::Rhi::Vulkan::Detail {
    namespace Container = Core::Container;
    using Container::FString;
    using Container::TVector;
    using Core::Memory::FBuddyAllocatorPolicy;
    using Core::Memory::FBuddyAllocation;
    using Core::Threading::FMutex;
    using Core::Threading::FScopedLock;

#if defined(AE_RHI_VULKAN_AVAILABLE) && AE_RHI_VULKAN_AVAILABLE
    struct FVulkanMemoryAllocation {
        VkDeviceMemory mMemory = VK_NULL_HANDLE;
        u64            mOffset = 0ULL;
        u64            mSize   = 0ULL;
        u32            mMemoryTypeIndex = 0U;
        void*          mMappedPtr = nullptr;

        FBuddyAllocation mSubAllocation{};
        void* mPool = nullptr;

        [[nodiscard]] auto IsValid() const noexcept -> bool {
            return mMemory != VK_NULL_HANDLE && mSize != 0ULL;
        }
    };

    struct FVulkanMemoryStats {
        u64 mTotalDeviceBytes = 0ULL;
        u64 mTotalUsedBytes   = 0ULL;
        u64 mAllocationCount  = 0ULL;
    };

    class FVulkanMemoryAllocator {
    public:
        FVulkanMemoryAllocator() = default;
        ~FVulkanMemoryAllocator();

        void Init(VkPhysicalDevice physicalDevice, VkDevice device);
        void Shutdown();

        auto Allocate(const VkMemoryRequirements& requirements, VkMemoryPropertyFlags flags,
            const FString& debugName) -> FVulkanMemoryAllocation;
        void Free(FVulkanMemoryAllocation& allocation);

        [[nodiscard]] auto GetStats() const noexcept -> FVulkanMemoryStats { return mStats; }

    private:
        struct FPool {
            VkDeviceMemory mMemory = VK_NULL_HANDLE;
            u64            mSize   = 0ULL;
            u32            mMemoryTypeIndex = 0U;
            bool           mHostVisible = false;
            void*          mMappedPtr = nullptr;
            FBuddyAllocatorPolicy mAllocator;
        };

        auto FindMemoryType(u32 typeBits, VkMemoryPropertyFlags flags) const noexcept -> u32;
        auto AllocateFromPool(FPool& pool, const VkMemoryRequirements& requirements)
            -> FVulkanMemoryAllocation;
        auto CreatePool(u32 memoryTypeIndex, u64 sizeBytes, bool hostVisible) -> FPool;

        VkPhysicalDevice mPhysicalDevice = VK_NULL_HANDLE;
        VkDevice         mDevice         = VK_NULL_HANDLE;
        VkPhysicalDeviceMemoryProperties mMemoryProps{};
        TVector<FPool> mPools;
        FVulkanMemoryStats mStats{};
        mutable FMutex mMutex;

        static constexpr u64 kDefaultDevicePoolSize = 256ULL * 1024ULL * 1024ULL;
        static constexpr u64 kDefaultHostPoolSize   = 64ULL * 1024ULL * 1024ULL;
        static constexpr u64 kDefaultMinBlockSize   = 256ULL;
    };
#endif
} // namespace AltinaEngine::Rhi::Vulkan::Detail
