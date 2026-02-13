#include "RhiD3D11/RhiD3D11BufferBacking.h"

#include "Platform/Generic/GenericPlatformDecl.h"

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
        auto ToD3D11Map(ED3D11MapMode mode) noexcept -> D3D11_MAP {
            switch (mode) {
                case ED3D11MapMode::WriteNoOverwrite:
                    return D3D11_MAP_WRITE_NO_OVERWRITE;
                case ED3D11MapMode::WriteDiscard:
                default:
                    return D3D11_MAP_WRITE_DISCARD;
            }
        }
    } // namespace
#endif

    FD3D11BufferBacking::FD3D11BufferBacking(
        ID3D11Buffer* buffer, ID3D11DeviceContext* context, u64 sizeBytes) {
        SetBuffer(buffer, context, sizeBytes);
    }

    void FD3D11BufferBacking::Reset() {
        EndWrite();
        mBuffer    = nullptr;
        mContext   = nullptr;
        mSizeBytes = 0ULL;
    }

    void FD3D11BufferBacking::SetBuffer(
        ID3D11Buffer* buffer, ID3D11DeviceContext* context, u64 sizeBytes) {
        EndWrite();
        mBuffer    = buffer;
        mContext   = context;
        mSizeBytes = sizeBytes;
    }

    auto FD3D11BufferBacking::IsValid() const noexcept -> bool {
        return (mBuffer != nullptr) && (mContext != nullptr) && (mSizeBytes != 0ULL);
    }

    auto FD3D11BufferBacking::GetBuffer() const noexcept -> ID3D11Buffer* { return mBuffer; }

    auto FD3D11BufferBacking::GetSizeBytes() const noexcept -> u64 { return mSizeBytes; }

    auto FD3D11BufferBacking::IsMapped() const noexcept -> bool { return mMappedData != nullptr; }

    void FD3D11BufferBacking::SetDefaultMapMode(ED3D11MapMode mode) { mDefaultMapMode = mode; }

    auto FD3D11BufferBacking::BeginWrite(ED3D11MapMode mode) -> bool {
#if AE_PLATFORM_WIN
        if (mMappedData != nullptr) {
            return true;
        }
        if (!IsValid()) {
            return false;
        }

        D3D11_MAPPED_SUBRESOURCE mapped = {};
        const HRESULT            hr     = mContext->Map(mBuffer, 0U, ToD3D11Map(mode), 0U, &mapped);
        if (FAILED(hr)) {
            return false;
        }
        mMappedData = static_cast<u8*>(mapped.pData);
        return (mMappedData != nullptr);
#else
        (void)mode;
        return false;
#endif
    }

    void FD3D11BufferBacking::EndWrite() {
#if AE_PLATFORM_WIN
        if (mMappedData == nullptr) {
            return;
        }
        if (mContext != nullptr && mBuffer != nullptr) {
            mContext->Unmap(mBuffer, 0U);
        }
#endif
        mMappedData = nullptr;
    }

    auto FD3D11BufferBacking::Write(u64 offset, const void* data, u64 sizeBytes) -> bool {
#if AE_PLATFORM_WIN
        if (!IsValid() || data == nullptr || sizeBytes == 0ULL) {
            return false;
        }
        if (offset > mSizeBytes || sizeBytes > (mSizeBytes - offset)) {
            return false;
        }

        if (mMappedData != nullptr) {
            AltinaEngine::Core::Platform::Generic::Memcpy(
                mMappedData + offset, data, static_cast<usize>(sizeBytes));
            return true;
        }

        D3D11_MAPPED_SUBRESOURCE mapped = {};
        const HRESULT hr = mContext->Map(mBuffer, 0U, ToD3D11Map(mDefaultMapMode), 0U, &mapped);
        if (FAILED(hr) || mapped.pData == nullptr) {
            return false;
        }
        auto* dst = static_cast<u8*>(mapped.pData);
        AltinaEngine::Core::Platform::Generic::Memcpy(
            dst + offset, data, static_cast<usize>(sizeBytes));
        mContext->Unmap(mBuffer, 0U);
        return true;
#else
        (void)offset;
        (void)data;
        (void)sizeBytes;
        return false;
#endif
    }
} // namespace AltinaEngine::Rhi
