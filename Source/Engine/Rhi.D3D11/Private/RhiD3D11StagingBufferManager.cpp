#include "RhiD3D11/RhiD3D11StagingBufferManager.h"

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
#endif

namespace AltinaEngine::Rhi {
#if AE_PLATFORM_WIN
    namespace {
        auto ToD3D11Map(ED3D11StagingMapMode mode) noexcept -> D3D11_MAP {
            switch (mode) {
            case ED3D11StagingMapMode::Write:
                return D3D11_MAP_WRITE;
            case ED3D11StagingMapMode::ReadWrite:
                return D3D11_MAP_READ_WRITE;
            case ED3D11StagingMapMode::Read:
            default:
                return D3D11_MAP_READ;
            }
        }
    } // namespace
#endif

    void FD3D11StagingBufferManager::Init(FRhiD3D11Device* device) {
        Reset();
        mDevice = device;
        if (mDevice == nullptr) {
            return;
        }
        mContext = mDevice->GetImmediateContext();
    }

    void FD3D11StagingBufferManager::Reset() {
        mEntries.Clear();
        mDevice = nullptr;
        mContext = nullptr;
    }

    auto FD3D11StagingBufferManager::Acquire(
        u64 sizeBytes, ERhiCpuAccess access) -> FD3D11StagingAllocation {
        if (mDevice == nullptr || sizeBytes == 0ULL || access == ERhiCpuAccess::None) {
            return {};
        }

        for (u32 i = 0; i < mEntries.Size(); ++i) {
            auto& entry = mEntries[i];
            if (entry.mInUse) {
                continue;
            }
            if (entry.mSizeBytes < sizeBytes) {
                continue;
            }
            if (entry.mCpuAccess != access) {
                continue;
            }
            entry.mInUse = true;
            FD3D11StagingAllocation allocation{};
            allocation.mBuffer = entry.mBuffer.Get();
            allocation.mSize   = entry.mSizeBytes;
            allocation.mPoolIndex = i;
            return allocation;
        }

        FRhiBufferDesc bufferDesc;
        bufferDesc.mSizeBytes = sizeBytes;
        bufferDesc.mUsage     = ERhiResourceUsage::Staging;
        bufferDesc.mCpuAccess = access;
        bufferDesc.mBindFlags = ERhiBufferBindFlags::None;

        FRhiBufferRef buffer = mDevice->CreateBuffer(bufferDesc);
        if (!buffer) {
            return {};
        }

        FStagingEntry entry{};
        entry.mBuffer = buffer;
        entry.mSizeBytes = sizeBytes;
        entry.mCpuAccess = access;
        entry.mInUse = true;
        const u32 poolIndex = static_cast<u32>(mEntries.Size());
        mEntries.PushBack(entry);

        FD3D11StagingAllocation allocation{};
        allocation.mBuffer = buffer.Get();
        allocation.mSize   = sizeBytes;
        allocation.mPoolIndex = poolIndex;
        return allocation;
    }

    void FD3D11StagingBufferManager::Release(const FD3D11StagingAllocation& allocation) {
        if (!allocation.IsValid()) {
            return;
        }
        if (allocation.mPoolIndex >= mEntries.Size()) {
            return;
        }
        mEntries[allocation.mPoolIndex].mInUse = false;
    }

    auto FD3D11StagingBufferManager::Map(
        const FD3D11StagingAllocation& allocation, ED3D11StagingMapMode mode) -> void* {
#if AE_PLATFORM_WIN
        if (!allocation.IsValid() || mContext == nullptr) {
            return nullptr;
        }
        auto* buffer = static_cast<FRhiD3D11Buffer*>(allocation.mBuffer);
        if (buffer == nullptr) {
            return nullptr;
        }
        ID3D11Buffer* nativeBuffer = buffer->GetNativeBuffer();
        if (nativeBuffer == nullptr) {
            return nullptr;
        }
        D3D11_MAPPED_SUBRESOURCE mapped = {};
        const HRESULT            hr =
            mContext->Map(nativeBuffer, 0U, ToD3D11Map(mode), 0U, &mapped);
        if (FAILED(hr)) {
            return nullptr;
        }
        return mapped.pData;
#else
        (void)allocation;
        (void)mode;
        return nullptr;
#endif
    }

    void FD3D11StagingBufferManager::Unmap(const FD3D11StagingAllocation& allocation) {
#if AE_PLATFORM_WIN
        if (!allocation.IsValid() || mContext == nullptr) {
            return;
        }
        auto* buffer = static_cast<FRhiD3D11Buffer*>(allocation.mBuffer);
        if (buffer == nullptr) {
            return;
        }
        ID3D11Buffer* nativeBuffer = buffer->GetNativeBuffer();
        if (nativeBuffer == nullptr) {
            return;
        }
        mContext->Unmap(nativeBuffer, 0U);
#else
        (void)allocation;
#endif
    }
} // namespace AltinaEngine::Rhi
