#include "RhiD3D11/RhiD3D11UploadBufferManager.h"

#include "RhiD3D11/RhiD3D11Device.h"
#include "RhiD3D11/RhiD3D11Resources.h"
#include "Rhi/RhiStructs.h"

#if AE_PLATFORM_WIN
    #ifdef TEXT
        #undef TEXT
    #endif
    #ifndef WIN32_LEAN_AND_MEAN
        #define WIN32_LEAN_AND_MEAN
    #endif
    #ifndef NOMINMAX
        #define NOMINMAX
    #endif
    #include <Windows.h>
    #ifdef CreateSemaphore
        #undef CreateSemaphore
    #endif
    #include <d3d11.h>
    #include <d3d11_1.h>
    #include <wrl/client.h>
#endif

namespace AltinaEngine::Rhi {
    namespace {
        constexpr u64      kConstantBufferAlign    = 16ULL;
        constexpr u64      kConstantBufferMaxBytes = 64ULL * 1024ULL;

        [[nodiscard]] auto AlignUp(u64 value, u64 alignment) noexcept -> u64 {
            if (alignment == 0ULL) {
                return value;
            }
            const u64 remainder = value % alignment;
            return (remainder == 0ULL) ? value : (value + (alignment - remainder));
        }
    } // namespace

    void FD3D11UploadBufferManager::Init(
        FRhiD3D11Device* device, const FD3D11UploadBufferManagerDesc& desc) {
        Reset();

        mDevice = device;
        if (mDevice == nullptr) {
            return;
        }

        mContext = mDevice->GetImmediateContext();
        if (mContext == nullptr) {
            return;
        }

        mPageSizeBytes = desc.mPageSizeBytes;
        if (mPageSizeBytes == 0ULL || desc.mPageCount == 0U) {
            return;
        }

        mAlignmentBytes = (desc.mAlignmentBytes == 0ULL) ? 16ULL : desc.mAlignmentBytes;
        mAllowConstantBufferSuballocation = desc.mAllowConstantBufferSuballocation;
        mPageSupportsConstant =
            mAllowConstantBufferSuballocation && (mPageSizeBytes <= kConstantBufferMaxBytes);
        if (mPageSupportsConstant) {
            mPageSizeBytes = AlignUp(mPageSizeBytes, kConstantBufferAlign);
            if (mPageSizeBytes > kConstantBufferMaxBytes) {
                mPageSupportsConstant = false;
            }
        }

#if AE_PLATFORM_WIN
        {
            Microsoft::WRL::ComPtr<ID3D11DeviceContext1> context1;
            if (SUCCEEDED(mContext->QueryInterface(IID_PPV_ARGS(&context1)))) {
                mSupportsConstantBufferSuballocation = true;
            }
        }
#endif

        mPages.Resize(desc.mPageCount);
        for (u32 i = 0; i < desc.mPageCount; ++i) {
            FRhiBufferDesc bufferDesc;
            bufferDesc.mSizeBytes = mPageSizeBytes;
            bufferDesc.mUsage     = ERhiResourceUsage::Dynamic;
            bufferDesc.mCpuAccess = ERhiCpuAccess::Write;
            bufferDesc.mBindFlags = ERhiBufferBindFlags::Vertex | ERhiBufferBindFlags::Index
                | ERhiBufferBindFlags::ShaderResource;
            if (mPageSupportsConstant) {
                bufferDesc.mBindFlags = bufferDesc.mBindFlags | ERhiBufferBindFlags::Constant;
            }

            FRhiBufferRef buffer = mDevice->CreateBuffer(bufferDesc);
            if (!buffer) {
                continue;
            }

            auto*         d3dBuffer    = static_cast<FRhiD3D11Buffer*>(buffer.Get());
            ID3D11Buffer* nativeBuffer = d3dBuffer ? d3dBuffer->GetNativeBuffer() : nullptr;

            FPage         page{};
            page.mBuffer    = buffer;
            page.mSizeBytes = mPageSizeBytes;
            page.mExecutor  = Core::Memory::TAllocatorExecutor<Core::Memory::FRingAllocatorPolicy,
                 FD3D11BufferBacking>(FD3D11BufferBacking(nativeBuffer, mContext, mPageSizeBytes));
            page.mExecutor.InitPolicyFromBacking();

            mPages[i] = AltinaEngine::Move(page);
        }
    }

    void FD3D11UploadBufferManager::Reset() {
        mPages.Clear();
        mConstantPool.Clear();
        mDevice                              = nullptr;
        mContext                             = nullptr;
        mPageSizeBytes                       = 0ULL;
        mAlignmentBytes                      = 16ULL;
        mFrameTag                            = 0ULL;
        mPageIndex                           = 0U;
        mAllowConstantBufferSuballocation    = false;
        mSupportsConstantBufferSuballocation = false;
        mPageSupportsConstant                = false;
    }

    void FD3D11UploadBufferManager::BeginFrame(u64 frameTag) {
        mFrameTag = frameTag;
        if (mPages.IsEmpty()) {
            return;
        }
        mPageIndex = static_cast<u32>(frameTag % mPages.Size());
        auto* page = GetCurrentPage();
        if (!page) {
            return;
        }
        page->mExecutor.Reset();
        page->mExecutor.GetBacking().BeginWrite(ED3D11MapMode::WriteDiscard);

        for (auto& slot : mConstantPool) {
            slot.mInUse = false;
        }
    }

    void FD3D11UploadBufferManager::EndFrame() {
        auto* page = GetCurrentPage();
        if (!page) {
            return;
        }
        page->mExecutor.GetBacking().EndWrite();
    }

    auto FD3D11UploadBufferManager::Allocate(u64 sizeBytes, u64 alignment, u64 tag)
        -> FD3D11UploadAllocation {
        auto* page = GetCurrentPage();
        if (page == nullptr) {
            return {};
        }
        const u64 actualAlignment = (alignment == 0ULL) ? mAlignmentBytes : alignment;
        auto      allocation      = page->mExecutor.Allocate(sizeBytes, actualAlignment, tag);
        if (!allocation.IsValid()) {
            return {};
        }
        FD3D11UploadAllocation result{};
        result.mBuffer = page->mBuffer.Get();
        result.mOffset = allocation.mOffset;
        result.mSize   = allocation.mSize;
        result.mTag    = allocation.mTag;
        return result;
    }

    auto FD3D11UploadBufferManager::AllocateConstant(u64 sizeBytes, u64 tag)
        -> FD3D11UploadAllocation {
        if (sizeBytes == 0ULL) {
            return {};
        }
        const u64 alignedSize = AlignUp(sizeBytes, kConstantBufferAlign);
        if (alignedSize > kConstantBufferMaxBytes) {
            return {};
        }

        if (SupportsConstantBufferSuballocation() && mPageSupportsConstant) {
            return Allocate(alignedSize, kConstantBufferAlign, tag);
        }

        for (auto& slot : mConstantPool) {
            if (slot.mInUse) {
                continue;
            }
            if (slot.mSizeBytes < alignedSize) {
                continue;
            }
            slot.mInUse = true;
            FD3D11UploadAllocation result{};
            result.mBuffer = slot.mBuffer.Get();
            result.mOffset = 0ULL;
            result.mSize   = alignedSize;
            result.mTag    = tag;
            return result;
        }

        if (mDevice == nullptr) {
            return {};
        }

        FRhiBufferDesc bufferDesc;
        bufferDesc.mSizeBytes = alignedSize;
        bufferDesc.mUsage     = ERhiResourceUsage::Dynamic;
        bufferDesc.mCpuAccess = ERhiCpuAccess::Write;
        bufferDesc.mBindFlags = ERhiBufferBindFlags::Constant;

        FRhiBufferRef buffer = mDevice->CreateBuffer(bufferDesc);
        if (!buffer) {
            return {};
        }

        FConstantBufferSlot slot{};
        slot.mBuffer    = buffer;
        slot.mSizeBytes = alignedSize;
        slot.mInUse     = true;
        mConstantPool.PushBack(slot);

        FD3D11UploadAllocation result{};
        result.mBuffer = buffer.Get();
        result.mOffset = 0ULL;
        result.mSize   = alignedSize;
        result.mTag    = tag;
        return result;
    }

    auto FD3D11UploadBufferManager::GetWritePointer(
        const FD3D11UploadAllocation& allocation, u64 dstOffset) -> void* {
        if (!allocation.IsValid()) {
            return nullptr;
        }
        auto* page = GetCurrentPage();
        if (page == nullptr || allocation.mBuffer != page->mBuffer.Get()) {
            return nullptr;
        }
        return page->mExecutor.GetWritePointer(allocation, dstOffset);
    }

    auto FD3D11UploadBufferManager::Write(const FD3D11UploadAllocation& allocation,
        const void* data, u64 sizeBytes, u64 dstOffset) -> bool {
        if (!allocation.IsValid() || data == nullptr || sizeBytes == 0ULL) {
            return false;
        }
        if (dstOffset > allocation.mSize || sizeBytes > (allocation.mSize - dstOffset)) {
            return false;
        }

        auto* page = GetCurrentPage();
        if (page && allocation.mBuffer == page->mBuffer.Get()) {
            return page->mExecutor.Write(allocation, data, sizeBytes, dstOffset);
        }

        auto* buffer = static_cast<FRhiD3D11Buffer*>(allocation.mBuffer);
        if (buffer == nullptr) {
            return false;
        }
        return WriteToBuffer(buffer, buffer->GetDesc().mSizeBytes, data, sizeBytes, dstOffset);
    }

    auto FD3D11UploadBufferManager::GetCurrentPage() -> FPage* {
        if (mPages.IsEmpty() || mPageIndex >= mPages.Size()) {
            return nullptr;
        }
        return &mPages[mPageIndex];
    }

    auto FD3D11UploadBufferManager::WriteToBuffer(FRhiBuffer* buffer, u64 bufferSizeBytes,
        const void* data, u64 sizeBytes, u64 dstOffset) -> bool {
        auto* d3dBuffer = static_cast<FRhiD3D11Buffer*>(buffer);
        if (d3dBuffer == nullptr || mContext == nullptr) {
            return false;
        }
        FD3D11BufferBacking backing(d3dBuffer->GetNativeBuffer(), mContext, bufferSizeBytes);
        return backing.Write(dstOffset, data, sizeBytes);
    }
} // namespace AltinaEngine::Rhi
