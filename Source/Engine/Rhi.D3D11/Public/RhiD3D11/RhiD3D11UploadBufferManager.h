#pragma once

#include "RhiD3D11API.h"
#include "Rhi/RhiRefs.h"
#include "Types/Aliases.h"
#include "Container/Vector.h"
#include "Memory/AllocatorExecutor.h"
#include "Memory/RingAllocatorPolicy.h"
#include "RhiD3D11/RhiD3D11BufferBacking.h"

struct ID3D11DeviceContext;

namespace AltinaEngine::Rhi {
    namespace Container = Core::Container;
    class FRhiD3D11Device;

    struct FD3D11UploadAllocation {
        FRhiBuffer*        mBuffer = nullptr;
        u64                mOffset = 0ULL;
        u64                mSize   = 0ULL;
        u64                mTag    = 0ULL;

        [[nodiscard]] auto IsValid() const noexcept -> bool { return mBuffer != nullptr; }
    };

    struct FD3D11UploadBufferManagerDesc {
        u64  mPageSizeBytes                    = 4ULL * 1024ULL * 1024ULL;
        u32  mPageCount                        = 3U;
        u64  mAlignmentBytes                   = 16ULL;
        bool mAllowConstantBufferSuballocation = false;
    };

    class AE_RHI_D3D11_API FD3D11UploadBufferManager {
    public:
        FD3D11UploadBufferManager() = default;

        void               Init(FRhiD3D11Device* device, const FD3D11UploadBufferManagerDesc& desc);
        void               Reset();

        void               BeginFrame(u64 frameTag = 0ULL);
        void               EndFrame();

        [[nodiscard]] auto SupportsConstantBufferSuballocation() const noexcept -> bool {
            return mSupportsConstantBufferSuballocation && mAllowConstantBufferSuballocation;
        }

        auto Allocate(u64 sizeBytes, u64 alignment, u64 tag = 0ULL) -> FD3D11UploadAllocation;
        auto AllocateConstant(u64 sizeBytes, u64 tag = 0ULL) -> FD3D11UploadAllocation;

        auto GetWritePointer(const FD3D11UploadAllocation& allocation, u64 dstOffset = 0ULL)
            -> void*;

        auto Write(const FD3D11UploadAllocation& allocation, const void* data, u64 sizeBytes,
            u64 dstOffset = 0ULL) -> bool;

    private:
        struct FPage {
            FRhiBufferRef mBuffer;
            Core::Memory::TAllocatorExecutor<Core::Memory::FRingAllocatorPolicy,
                FD3D11BufferBacking>
                mExecutor;
            u64 mSizeBytes = 0ULL;
        };

        struct FConstantBufferSlot {
            FRhiBufferRef mBuffer;
            u64           mSizeBytes = 0ULL;
            bool          mInUse     = false;
        };

        auto GetCurrentPage() -> FPage*;
        auto WriteToBuffer(FRhiBuffer* buffer, u64 bufferSizeBytes, const void* data, u64 sizeBytes,
            u64 dstOffset) -> bool;

        FRhiD3D11Device*                        mDevice  = nullptr;
        ::ID3D11DeviceContext*                  mContext = nullptr;
        Container::TVector<FPage>               mPages;
        Container::TVector<FConstantBufferSlot> mConstantPool;
        u64                                     mPageSizeBytes                       = 0ULL;
        u64                                     mAlignmentBytes                      = 16ULL;
        u64                                     mFrameTag                            = 0ULL;
        u32                                     mPageIndex                           = 0U;
        bool                                    mAllowConstantBufferSuballocation    = false;
        bool                                    mSupportsConstantBufferSuballocation = false;
        bool                                    mPageSupportsConstant                = false;
    };
} // namespace AltinaEngine::Rhi
