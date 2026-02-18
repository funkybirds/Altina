#include "RhiVulkanMemoryAllocator.h"

namespace AltinaEngine::Rhi::Vulkan::Detail {
#if defined(AE_RHI_VULKAN_AVAILABLE) && AE_RHI_VULKAN_AVAILABLE
    FVulkanMemoryAllocator::~FVulkanMemoryAllocator() { Shutdown(); }

    void FVulkanMemoryAllocator::Init(VkPhysicalDevice physicalDevice, VkDevice device) {
        Shutdown();
        mPhysicalDevice = physicalDevice;
        mDevice         = device;
        if (mPhysicalDevice != VK_NULL_HANDLE) {
            vkGetPhysicalDeviceMemoryProperties(mPhysicalDevice, &mMemoryProps);
        }
    }

    void FVulkanMemoryAllocator::Shutdown() {
        if (mDevice != VK_NULL_HANDLE) {
            for (auto& pool : mPools) {
                if (pool.mMemory != VK_NULL_HANDLE) {
                    if (pool.mMappedPtr) {
                        vkUnmapMemory(mDevice, pool.mMemory);
                        pool.mMappedPtr = nullptr;
                    }
                    vkFreeMemory(mDevice, pool.mMemory, nullptr);
                }
            }
        }
        mPools.Clear();
        mStats = {};
        mDevice = VK_NULL_HANDLE;
        mPhysicalDevice = VK_NULL_HANDLE;
    }

    auto FVulkanMemoryAllocator::FindMemoryType(u32 typeBits, VkMemoryPropertyFlags flags) const
        noexcept -> u32 {
        for (u32 i = 0; i < mMemoryProps.memoryTypeCount; ++i) {
            if ((typeBits & (1u << i)) == 0u) {
                continue;
            }
            if ((mMemoryProps.memoryTypes[i].propertyFlags & flags) == flags) {
                return i;
            }
        }
        return UINT32_MAX;
    }

    auto FVulkanMemoryAllocator::CreatePool(
        u32 memoryTypeIndex, u64 sizeBytes, bool hostVisible) -> FPool {
        FPool pool;
        pool.mMemoryTypeIndex = memoryTypeIndex;
        pool.mSize            = sizeBytes;
        pool.mHostVisible     = hostVisible;
        pool.mAllocator.Init(sizeBytes, kDefaultMinBlockSize);

        VkMemoryAllocateInfo allocInfo{};
        allocInfo.sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocInfo.allocationSize  = sizeBytes;
        allocInfo.memoryTypeIndex = memoryTypeIndex;

        if (vkAllocateMemory(mDevice, &allocInfo, nullptr, &pool.mMemory) != VK_SUCCESS) {
            pool.mMemory = VK_NULL_HANDLE;
            return pool;
        }

        if (hostVisible) {
            if (vkMapMemory(mDevice, pool.mMemory, 0, sizeBytes, 0, &pool.mMappedPtr)
                != VK_SUCCESS) {
                vkFreeMemory(mDevice, pool.mMemory, nullptr);
                pool.mMemory = VK_NULL_HANDLE;
                pool.mMappedPtr = nullptr;
                return pool;
            }
        }

        mStats.mTotalDeviceBytes += sizeBytes;
        return pool;
    }

    auto FVulkanMemoryAllocator::AllocateFromPool(
        FPool& pool, const VkMemoryRequirements& requirements) -> FVulkanMemoryAllocation {
        FVulkanMemoryAllocation allocation{};
        if (pool.mMemory == VK_NULL_HANDLE) {
            return allocation;
        }

        auto subAlloc = pool.mAllocator.Allocate(requirements.size, requirements.alignment);
        if (!subAlloc.IsValid()) {
            return allocation;
        }

        allocation.mMemory          = pool.mMemory;
        allocation.mOffset          = subAlloc.mOffset;
        allocation.mSize            = subAlloc.mSize;
        allocation.mMemoryTypeIndex = pool.mMemoryTypeIndex;
        allocation.mSubAllocation   = subAlloc;
        allocation.mPool            = &pool;
        if (pool.mMappedPtr) {
            allocation.mMappedPtr = static_cast<u8*>(pool.mMappedPtr) + allocation.mOffset;
        }

        mStats.mTotalUsedBytes += allocation.mSize;
        mStats.mAllocationCount += 1ULL;
        return allocation;
    }

    auto FVulkanMemoryAllocator::Allocate(const VkMemoryRequirements& requirements,
        VkMemoryPropertyFlags flags, const FString& /*debugName*/) -> FVulkanMemoryAllocation {
        FScopedLock lock(mMutex);
        if (mDevice == VK_NULL_HANDLE) {
            return {};
        }

        const u32 memoryTypeIndex = FindMemoryType(requirements.memoryTypeBits, flags);
        if (memoryTypeIndex == UINT32_MAX) {
            return {};
        }

        const bool hostVisible = (flags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) != 0;

        for (auto& pool : mPools) {
            if (pool.mMemoryTypeIndex != memoryTypeIndex || pool.mHostVisible != hostVisible) {
                continue;
            }
            auto alloc = AllocateFromPool(pool, requirements);
            if (alloc.IsValid()) {
                return alloc;
            }
        }

        const u64 poolSize = hostVisible ? kDefaultHostPoolSize : kDefaultDevicePoolSize;
        auto      pool     = CreatePool(memoryTypeIndex, poolSize, hostVisible);
        if (pool.mMemory == VK_NULL_HANDLE) {
            return {};
        }

        mPools.PushBack(pool);
        return AllocateFromPool(mPools.Back(), requirements);
    }

    void FVulkanMemoryAllocator::Free(FVulkanMemoryAllocation& allocation) {
        if (!allocation.IsValid()) {
            allocation = {};
            return;
        }

        FScopedLock lock(mMutex);
        auto* pool = static_cast<FPool*>(allocation.mPool);
        if (pool != nullptr) {
            pool->mAllocator.Free(allocation.mSubAllocation);
        }
        if (mStats.mTotalUsedBytes >= allocation.mSize) {
            mStats.mTotalUsedBytes -= allocation.mSize;
        } else {
            mStats.mTotalUsedBytes = 0ULL;
        }
        if (mStats.mAllocationCount > 0ULL) {
            mStats.mAllocationCount -= 1ULL;
        }
        allocation = {};
    }
#endif
} // namespace AltinaEngine::Rhi::Vulkan::Detail
