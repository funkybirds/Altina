#include "RhiVulkan/RhiVulkanUploadBufferManager.h"

#include "RhiVulkan/RhiVulkanDevice.h"
#include "Rhi/RhiBuffer.h"
#include "Rhi/RhiEnums.h"

namespace AltinaEngine::Rhi {
    namespace Container = Core::Container;

    namespace {
        [[nodiscard]] auto AlignUp(u64 value, u64 alignment) noexcept -> u64 {
            if (alignment == 0ULL) {
                return value;
            }
            const u64 remainder = value % alignment;
            return (remainder == 0ULL) ? value : (value + (alignment - remainder));
        }
    } // namespace

    void FVulkanUploadBufferManager::Init(
        FRhiVulkanDevice* device, const FVulkanUploadBufferManagerDesc& desc) {
        Reset();

        mDevice = device;
        if (mDevice == nullptr) {
            return;
        }

        mPageSizeBytes  = desc.mPageSizeBytes;
        mAlignmentBytes = (desc.mAlignmentBytes == 0ULL) ? 16ULL : desc.mAlignmentBytes;
        if (mPageSizeBytes == 0ULL || desc.mPageCount == 0U) {
            return;
        }

        mPages.Resize(desc.mPageCount);
        for (u32 i = 0; i < desc.mPageCount; ++i) {
            FRhiBufferDesc bufferDesc{};
            bufferDesc.mSizeBytes = mPageSizeBytes;
            bufferDesc.mUsage     = ERhiResourceUsage::Dynamic;
            bufferDesc.mCpuAccess = ERhiCpuAccess::Write;
            bufferDesc.mBindFlags = ERhiBufferBindFlags::Vertex | ERhiBufferBindFlags::Index
                | ERhiBufferBindFlags::Constant | ERhiBufferBindFlags::ShaderResource
                | ERhiBufferBindFlags::CopySrc;

            FRhiBufferRef buffer = mDevice->CreateBuffer(bufferDesc);
            if (!buffer) {
                continue;
            }

            FPage page{};
            page.mBuffer    = buffer;
            page.mSizeBytes = mPageSizeBytes;
            page.mRing.Init(mPageSizeBytes);
            mPages[i] = page;
        }

        BeginFrame(0ULL);
    }

    void FVulkanUploadBufferManager::Reset() {
        mPages.Clear();
        mDevice         = nullptr;
        mPageSizeBytes  = 0ULL;
        mAlignmentBytes = 16ULL;
        mFrameTag       = 0ULL;
        mPageIndex      = 0U;
    }

    void FVulkanUploadBufferManager::BeginFrame(u64 frameTag) {
        mFrameTag = frameTag;
        if (mPages.IsEmpty()) {
            return;
        }

        mPageIndex = static_cast<u32>(frameTag % mPages.Size());
        auto* page = GetCurrentPage();
        if (!page || !page->mBuffer) {
            return;
        }

        page->mRing.Reset();
        page->mLock = page->mBuffer->Lock(0ULL, page->mSizeBytes, ERhiBufferLockMode::WriteDiscard);
    }

    void FVulkanUploadBufferManager::EndFrame() {
        auto* page = GetCurrentPage();
        if (!page || !page->mBuffer) {
            return;
        }
        if (page->mLock.IsValid()) {
            page->mBuffer->Unlock(page->mLock);
            page->mLock = {};
        }
    }

    auto FVulkanUploadBufferManager::GetCurrentPage() -> FPage* {
        if (mPages.IsEmpty() || mPageIndex >= static_cast<u32>(mPages.Size())) {
            return nullptr;
        }
        return &mPages[mPageIndex];
    }

    auto FVulkanUploadBufferManager::Allocate(u64 sizeBytes, u64 alignment, u64 tag)
        -> FVulkanUploadAllocation {
        auto* page = GetCurrentPage();
        if (!page || !page->mBuffer) {
            return {};
        }

        const u64 actualAlignment = (alignment == 0ULL) ? mAlignmentBytes : alignment;
        const u64 alignedSize     = AlignUp(sizeBytes, actualAlignment);
        auto      alloc           = page->mRing.Allocate(alignedSize, actualAlignment, tag);
        if (!alloc.IsValid()) {
            return {};
        }

        FVulkanUploadAllocation out{};
        out.mBuffer = page->mBuffer.Get();
        out.mOffset = alloc.mOffset;
        out.mSize   = alloc.mSize;
        out.mTag    = alloc.mTag;
        return out;
    }

    auto FVulkanUploadBufferManager::GetWritePointer(
        const FVulkanUploadAllocation& allocation, u64 dstOffset) -> void* {
        if (!allocation.IsValid()) {
            return nullptr;
        }
        auto* page = GetCurrentPage();
        if (!page || allocation.mBuffer != page->mBuffer.Get() || !page->mLock.IsValid()) {
            return nullptr;
        }
        if (dstOffset >= allocation.mSize) {
            return nullptr;
        }
        auto* base = static_cast<u8*>(page->mLock.mData);
        return base ? (base + allocation.mOffset + dstOffset) : nullptr;
    }

    auto FVulkanUploadBufferManager::Write(const FVulkanUploadAllocation& allocation,
        const void* data, u64 sizeBytes, u64 dstOffset) -> bool {
        if (!allocation.IsValid() || data == nullptr || sizeBytes == 0ULL) {
            return false;
        }
        if (dstOffset > allocation.mSize || sizeBytes > (allocation.mSize - dstOffset)) {
            return false;
        }
        void* dst = GetWritePointer(allocation, dstOffset);
        if (dst == nullptr) {
            return false;
        }
        Core::Platform::Generic::Memcpy(dst, data, static_cast<usize>(sizeBytes));
        return true;
    }
} // namespace AltinaEngine::Rhi
