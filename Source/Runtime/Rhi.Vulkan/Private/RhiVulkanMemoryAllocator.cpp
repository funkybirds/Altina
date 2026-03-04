#include "RhiVulkanMemoryAllocator.h"
#include "RhiVulkanDebugUtils.h"

namespace AltinaEngine::Rhi::Vulkan::Detail {
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
        mStats          = {};
        mDevice         = VK_NULL_HANDLE;
        mPhysicalDevice = VK_NULL_HANDLE;
    }

    auto FVulkanMemoryAllocator::FindMemoryType(
        u32 typeBits, VkMemoryPropertyFlags flags) const noexcept -> u32 {
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

    auto FVulkanMemoryAllocator::CreatePool(u32 memoryTypeIndex, u64 sizeBytes, bool hostVisible)
        -> FPool {
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
                pool.mMemory    = VK_NULL_HANDLE;
                pool.mMappedPtr = nullptr;
                return pool;
            }
        }

        mStats.mTotalDeviceBytes += sizeBytes;
        return pool;
    }

    auto FVulkanMemoryAllocator::AllocateFromPool(FPool& pool, u32 poolIndex,
        const VkMemoryRequirements& requirements) -> FVulkanMemoryAllocation {
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
        allocation.mPoolIndex       = poolIndex;
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

        for (u32 i = 0; i < static_cast<u32>(mPools.Size()); ++i) {
            auto& pool = mPools[static_cast<usize>(i)];
            if (pool.mMemoryTypeIndex != memoryTypeIndex || pool.mHostVisible != hostVisible) {
                continue;
            }
            auto alloc = AllocateFromPool(pool, i, requirements);
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
        const u32 newPoolIndex = static_cast<u32>(mPools.Size() - 1U);
        FString   poolDebugName;
        poolDebugName.Append(TEXT("RhiVulkan.MemoryPool.Type"));
        poolDebugName.Append(FString::ToString(memoryTypeIndex));
        poolDebugName.Append(TEXT(".Index"));
        poolDebugName.Append(FString::ToString(newPoolIndex));
        Vulkan::Detail::SetVkObjectDebugName(mDevice, mPools.Back().mMemory,
            VK_OBJECT_TYPE_DEVICE_MEMORY, poolDebugName.ToView(), TEXT("RhiVulkan.MemoryPool"),
            TEXT("VkDeviceMemory"));
        return AllocateFromPool(mPools.Back(), newPoolIndex, requirements);
    }

    void FVulkanMemoryAllocator::Free(FVulkanMemoryAllocation& allocation) {
        if (!allocation.IsValid()) {
            allocation = {};
            return;
        }

        FScopedLock lock(mMutex);
        if (allocation.mPoolIndex < static_cast<u32>(mPools.Size())) {
            auto& pool = mPools[static_cast<usize>(allocation.mPoolIndex)];
            if (pool.mMemory == allocation.mMemory) {
                (void)pool.mAllocator.Free(allocation.mSubAllocation);
            }
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
} // namespace AltinaEngine::Rhi::Vulkan::Detail
