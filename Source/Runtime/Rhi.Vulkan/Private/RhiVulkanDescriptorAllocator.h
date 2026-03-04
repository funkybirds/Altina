#pragma once

#include "RhiVulkanInternal.h"
#include "Container/HashMap.h"
#include "Container/String.h"
#include "Container/Vector.h"
#include "Rhi/RhiStructs.h"

namespace AltinaEngine::Rhi {
    namespace Container = Core::Container;
    using Container::FString;
    using Container::THashMap;
    using Container::TVector;

    struct FVulkanDescriptorAlloc {
        VkDescriptorPool mPool = VK_NULL_HANDLE;
        VkDescriptorSet  mSet  = VK_NULL_HANDLE;
    };

    class FVulkanDescriptorAllocator {
    public:
        FVulkanDescriptorAllocator() = default;
        ~FVulkanDescriptorAllocator();

        void Init(VkDevice device) noexcept;
        auto Allocate(u64 layoutHash, VkDescriptorSetLayout setLayout,
            const FRhiBindGroupLayoutDesc& layoutDesc) noexcept -> FVulkanDescriptorAlloc;
        void Free(VkDescriptorPool pool, VkDescriptorSet set) noexcept;
        void Shutdown() noexcept;

    private:
        struct FLayoutBucket {
            VkDescriptorSetLayout         mSetLayout = VK_NULL_HANDLE;
            TVector<VkDescriptorPool>     mPools;
            TVector<VkDescriptorPoolSize> mPoolSizes;
            FString                       mDebugName;
            u32                           mNextPoolIndex = 0U;
            u32                           mNextSetIndex  = 0U;
        };

        auto     GetOrCreateBucket(u64 layoutHash, VkDescriptorSetLayout setLayout,
                const FRhiBindGroupLayoutDesc& layoutDesc) noexcept -> FLayoutBucket*;
        auto     CreatePool(FLayoutBucket& bucket) noexcept -> VkDescriptorPool;
        void     BuildPoolSizes(const FRhiBindGroupLayoutDesc& layoutDesc,
                TVector<VkDescriptorPoolSize>&                 outPoolSizes) const noexcept;

        VkDevice mDevice = VK_NULL_HANDLE;
        THashMap<u64, TVector<FLayoutBucket>> mBuckets;
        u32                                   mSetsPerPool = 32U;
    };
} // namespace AltinaEngine::Rhi
