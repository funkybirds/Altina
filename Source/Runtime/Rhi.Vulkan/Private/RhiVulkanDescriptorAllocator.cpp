#include "RhiVulkanDescriptorAllocator.h"
#include "RhiVulkanDebugUtils.h"

namespace AltinaEngine::Rhi {
    namespace {
        [[nodiscard]] auto ToVkDescriptorType(ERhiBindingType type, bool dynamicOffset) noexcept
            -> VkDescriptorType {
            switch (type) {
                case ERhiBindingType::ConstantBuffer:
                    return dynamicOffset ? VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC
                                         : VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
                case ERhiBindingType::SampledTexture:
                    return VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
                case ERhiBindingType::StorageTexture:
                    return VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
                case ERhiBindingType::Sampler:
                    return VK_DESCRIPTOR_TYPE_SAMPLER;
                case ERhiBindingType::SampledBuffer:
                case ERhiBindingType::StorageBuffer:
                    return VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
                default:
                    return VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
            }
        }
    } // namespace

    FVulkanDescriptorAllocator::~FVulkanDescriptorAllocator() { Shutdown(); }

    void FVulkanDescriptorAllocator::Init(VkDevice device) noexcept { mDevice = device; }

    auto FVulkanDescriptorAllocator::GetOrCreateBucket(u64 layoutHash,
        VkDescriptorSetLayout setLayout, const FRhiBindGroupLayoutDesc& layoutDesc) noexcept
        -> FLayoutBucket* {
        u64 key = layoutHash;
        if (key == 0ULL) {
            key            = 1469598103934665603ULL;
            const auto mix = [&key](u64 value) {
                key ^= value;
                key *= 1099511628211ULL;
            };
            mix(static_cast<u64>(layoutDesc.mSetIndex));
            mix(static_cast<u64>(layoutDesc.mEntries.Size()));
            for (const auto& entry : layoutDesc.mEntries) {
                mix(static_cast<u64>(entry.mBinding));
                mix(static_cast<u64>(entry.mType));
                mix(static_cast<u64>(entry.mVisibility));
                mix(static_cast<u64>(entry.mArrayCount));
                mix(entry.mHasDynamicOffset ? 1ULL : 0ULL);
            }
        }
        auto it = mBuckets.FindIt(key);
        if (it == mBuckets.end()) {
            auto inserted = mBuckets.Emplace(key, TVector<FLayoutBucket>{});
            it            = inserted.first;
        }

        auto& buckets = it->second;
        for (auto& bucket : buckets) {
            if (bucket.mSetLayout == setLayout) {
                return &bucket;
            }
        }

        FLayoutBucket bucket{};
        bucket.mSetLayout = setLayout;
        if (!layoutDesc.mDebugName.IsEmptyString()) {
            bucket.mDebugName.Assign(layoutDesc.mDebugName);
        } else {
            bucket.mDebugName.Assign(TEXT("RhiVulkan.BindGroupLayout"));
        }
        BuildPoolSizes(layoutDesc, bucket.mPoolSizes);
        buckets.PushBack(bucket);
        return &buckets.Back();
    }

    auto FVulkanDescriptorAllocator::CreatePool(FLayoutBucket& bucket) noexcept
        -> VkDescriptorPool {
        if (mDevice == VK_NULL_HANDLE) {
            return VK_NULL_HANDLE;
        }

        VkDescriptorPoolCreateInfo poolInfo{};
        poolInfo.sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        poolInfo.flags         = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
        poolInfo.maxSets       = mSetsPerPool;
        poolInfo.poolSizeCount = static_cast<u32>(bucket.mPoolSizes.Size());
        poolInfo.pPoolSizes    = bucket.mPoolSizes.IsEmpty() ? nullptr : bucket.mPoolSizes.Data();

        VkDescriptorPool pool = VK_NULL_HANDLE;
        if (vkCreateDescriptorPool(mDevice, &poolInfo, nullptr, &pool) != VK_SUCCESS) {
            return VK_NULL_HANDLE;
        }

        FString poolDebugName;
        poolDebugName.Append(bucket.mDebugName);
        poolDebugName.Append(TEXT(".DescriptorPool."));
        poolDebugName.Append(FString::ToString(bucket.mNextPoolIndex++));
        Vulkan::Detail::SetVkObjectDebugName(mDevice, pool, VK_OBJECT_TYPE_DESCRIPTOR_POOL,
            poolDebugName.ToView(), TEXT("RhiVulkan.DescriptorPool"), TEXT("VkDescriptorPool"));

        bucket.mPools.PushBack(pool);
        return pool;
    }

    auto FVulkanDescriptorAllocator::Allocate(u64 layoutHash, VkDescriptorSetLayout setLayout,
        const FRhiBindGroupLayoutDesc& layoutDesc) noexcept -> FVulkanDescriptorAlloc {
        FVulkanDescriptorAlloc out{};
        if (mDevice == VK_NULL_HANDLE || setLayout == VK_NULL_HANDLE) {
            return out;
        }

        auto* bucket = GetOrCreateBucket(layoutHash, setLayout, layoutDesc);
        if (bucket == nullptr) {
            return out;
        }

        auto tryAllocateFromPool = [&](VkDescriptorPool pool) -> bool {
            VkDescriptorSetAllocateInfo allocInfo{};
            allocInfo.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
            allocInfo.descriptorPool     = pool;
            allocInfo.descriptorSetCount = 1;
            allocInfo.pSetLayouts        = &setLayout;
            if (vkAllocateDescriptorSets(mDevice, &allocInfo, &out.mSet) == VK_SUCCESS) {
                out.mPool = pool;
                FString setDebugName;
                setDebugName.Append(bucket->mDebugName);
                setDebugName.Append(TEXT(".DescriptorSet."));
                setDebugName.Append(FString::ToString(bucket->mNextSetIndex++));
                Vulkan::Detail::SetVkObjectDebugName(mDevice, out.mSet,
                    VK_OBJECT_TYPE_DESCRIPTOR_SET, setDebugName.ToView(),
                    TEXT("RhiVulkan.DescriptorSet"), TEXT("VkDescriptorSet"));
                return true;
            }
            return false;
        };

        for (auto pool : bucket->mPools) {
            if (tryAllocateFromPool(pool)) {
                return out;
            }
        }

        VkDescriptorPool newPool = CreatePool(*bucket);
        if (newPool == VK_NULL_HANDLE) {
            return out;
        }
        (void)tryAllocateFromPool(newPool);
        return out;
    }

    void FVulkanDescriptorAllocator::Free(VkDescriptorPool pool, VkDescriptorSet set) noexcept {
        if (mDevice == VK_NULL_HANDLE || pool == VK_NULL_HANDLE || set == VK_NULL_HANDLE) {
            return;
        }
        vkFreeDescriptorSets(mDevice, pool, 1, &set);
    }

    void FVulkanDescriptorAllocator::BuildPoolSizes(const FRhiBindGroupLayoutDesc& layoutDesc,
        TVector<VkDescriptorPoolSize>& outPoolSizes) const noexcept {
        outPoolSizes.Clear();
        outPoolSizes.Reserve(layoutDesc.mEntries.Size());

        auto addPoolSize = [&outPoolSizes](VkDescriptorType type, u32 count) -> void {
            for (auto& poolSize : outPoolSizes) {
                if (poolSize.type == type) {
                    poolSize.descriptorCount += count;
                    return;
                }
            }
            VkDescriptorPoolSize poolSize{};
            poolSize.type            = type;
            poolSize.descriptorCount = count;
            outPoolSizes.PushBack(poolSize);
        };

        for (const auto& entry : layoutDesc.mEntries) {
            addPoolSize(ToVkDescriptorType(entry.mType, entry.mHasDynamicOffset),
                entry.mArrayCount * mSetsPerPool);
        }
    }

    void FVulkanDescriptorAllocator::Shutdown() noexcept {
        if (mDevice == VK_NULL_HANDLE) {
            return;
        }
        for (auto& entry : mBuckets) {
            auto& buckets = entry.second;
            for (auto& bucket : buckets) {
                for (auto pool : bucket.mPools) {
                    if (pool != VK_NULL_HANDLE) {
                        vkDestroyDescriptorPool(mDevice, pool, nullptr);
                    }
                }
                bucket.mPools.Clear();
            }
        }
        mBuckets.Clear();
        mDevice = VK_NULL_HANDLE;
    }
} // namespace AltinaEngine::Rhi
