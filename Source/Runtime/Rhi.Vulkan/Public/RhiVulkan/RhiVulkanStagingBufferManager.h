#pragma once

#include "RhiVulkanAPI.h"
#include "Rhi/RhiEnums.h"
#include "Rhi/RhiBuffer.h"
#include "Rhi/RhiRefs.h"
#include "Types/Aliases.h"
#include "Container/Vector.h"

namespace AltinaEngine::Rhi {
    class FRhiVulkanDevice;

    enum class EVulkanStagingMapMode : u8 {
        Read,
        Write,
        ReadWrite
    };

    struct FVulkanStagingAllocation {
        FRhiBuffer*        mBuffer    = nullptr;
        u64                mSize      = 0ULL;
        u32                mPoolIndex = 0U;

        [[nodiscard]] auto IsValid() const noexcept -> bool { return mBuffer != nullptr; }
    };

    class AE_RHI_VULKAN_API FVulkanStagingBufferManager {
    public:
        FVulkanStagingBufferManager() = default;

        void Init(FRhiVulkanDevice* device);
        void Reset();
        void Shutdown();

        auto Acquire(u64 sizeBytes, ERhiCpuAccess access) -> FVulkanStagingAllocation;
        void Release(const FVulkanStagingAllocation& allocation);

        auto Map(const FVulkanStagingAllocation& allocation, EVulkanStagingMapMode mode) -> void*;
        void Unmap(const FVulkanStagingAllocation& allocation);

    private:
        struct FStagingEntry {
            FRhiBufferRef           mBuffer;
            u64                     mSizeBytes = 0ULL;
            ERhiCpuAccess           mCpuAccess = ERhiCpuAccess::Read;
            bool                    mInUse     = false;
            FRhiBuffer::FLockResult mLock{};
        };

        FRhiVulkanDevice*                       mDevice = nullptr;
        Core::Container::TVector<FStagingEntry> mEntries;
    };
} // namespace AltinaEngine::Rhi
