#pragma once

#include "RhiVulkanAPI.h"
#include "Rhi/RhiRefs.h"
#include "Rhi/RhiBuffer.h"
#include "Memory/RingAllocatorPolicy.h"
#include "Platform/Generic/GenericPlatformDecl.h"
#include "Types/Aliases.h"
#include "Container/Vector.h"

namespace AltinaEngine::Rhi {
    class FRhiVulkanDevice;

    struct FVulkanUploadAllocation {
        FRhiBuffer*        mBuffer = nullptr;
        u64                mOffset = 0ULL;
        u64                mSize   = 0ULL;
        u64                mTag    = 0ULL;

        [[nodiscard]] auto IsValid() const noexcept -> bool { return mBuffer != nullptr; }
    };

    struct FVulkanUploadBufferManagerDesc {
        u64 mPageSizeBytes  = 4ULL * 1024ULL * 1024ULL;
        u32 mPageCount      = 3U;
        u64 mAlignmentBytes = 16ULL;
    };

    // Vulkan-side equivalent of the D3D11 upload buffer manager: per-frame ring pages backed by
    // host-visible buffers, used for dynamic updates and resource uploads.
    class AE_RHI_VULKAN_API FVulkanUploadBufferManager {
    public:
        FVulkanUploadBufferManager() = default;

        void Init(FRhiVulkanDevice* device, const FVulkanUploadBufferManagerDesc& desc);
        void Reset();

        void BeginFrame(u64 frameTag = 0ULL);
        void EndFrame();

        auto Allocate(u64 sizeBytes, u64 alignment, u64 tag = 0ULL) -> FVulkanUploadAllocation;

        auto GetWritePointer(const FVulkanUploadAllocation& allocation, u64 dstOffset = 0ULL)
            -> void*;

        auto Write(const FVulkanUploadAllocation& allocation, const void* data, u64 sizeBytes,
            u64 dstOffset = 0ULL) -> bool;

    private:
        struct FPage {
            FRhiBufferRef                      mBuffer;
            u64                                mSizeBytes = 0ULL;
            Core::Memory::FRingAllocatorPolicy mRing;
            FRhiBuffer::FLockResult            mLock{};
        };

        auto                            GetCurrentPage() -> FPage*;

        FRhiVulkanDevice*               mDevice = nullptr;
        Core::Container::TVector<FPage> mPages;
        u64                             mPageSizeBytes  = 0ULL;
        u64                             mAlignmentBytes = 16ULL;
        u64                             mFrameTag       = 0ULL;
        u32                             mPageIndex      = 0U;
    };
} // namespace AltinaEngine::Rhi
