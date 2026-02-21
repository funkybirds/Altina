#include "RhiD3D11/RhiD3D11Device.h"
#include "RhiD3D11/RhiD3D11Resources.h"
#include "RhiD3D11/RhiD3D11StagingBufferManager.h"
#include "RhiD3D11/RhiD3D11UploadBufferManager.h"

#include "Rhi/RhiBuffer.h"
#include "Rhi/RhiInit.h"
#include "Rhi/RhiSampler.h"
#include "Rhi/RhiTexture.h"
#include "Types/Aliases.h"
#include "Types/Traits.h"

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
    #include <wrl/client.h>
#endif

#include <limits>

namespace AltinaEngine::Rhi {
    namespace Container = Core::Container;
#if AE_PLATFORM_WIN
    using Microsoft::WRL::ComPtr;

    struct FRhiD3D11Buffer::FState {
        ComPtr<ID3D11Buffer>              mBuffer;
        ComPtr<ID3D11ShaderResourceView>  mSrv;
        ComPtr<ID3D11UnorderedAccessView> mUav;
    };

    struct FRhiD3D11Texture::FState {
        ComPtr<ID3D11Resource>            mResource;
        ComPtr<ID3D11RenderTargetView>    mRtv;
        ComPtr<ID3D11DepthStencilView>    mDsv;
        ComPtr<ID3D11ShaderResourceView>  mSrv;
        ComPtr<ID3D11UnorderedAccessView> mUav;
    };

    struct FRhiD3D11Sampler::FState {
        ComPtr<ID3D11SamplerState> mSampler;
    };
#else
    struct FRhiD3D11Buffer::FState {};
    struct FRhiD3D11Texture::FState {};
    struct FRhiD3D11Sampler::FState {};
#endif

    using Container::MakeUnique;

#if AE_PLATFORM_WIN
    enum class ED3D11BufferLockPath : u8 {
        None,
        DirectMap,
        Upload,
        Staging
    };

    namespace {
        [[nodiscard]] auto ToD3D11Map(ERhiBufferLockMode mode) noexcept -> D3D11_MAP;
        [[nodiscard]] auto BuildBufferBox(u64 offset, u64 size, D3D11_BOX& box) noexcept -> bool;
    } // namespace

    struct FD3D11BufferLockState {
        ED3D11BufferLockPath    mPath         = ED3D11BufferLockPath::None;
        FRhiD3D11Device*        mDevice       = nullptr;
        ID3D11DeviceContext*    mContext      = nullptr;
        ID3D11Buffer*           mNativeBuffer = nullptr;
        FD3D11UploadAllocation  mUpload;
        FD3D11StagingAllocation mStaging;
    };
#endif

    FRhiD3D11Buffer::FRhiD3D11Buffer(const FRhiBufferDesc& desc, ID3D11Buffer* buffer,
        ID3D11ShaderResourceView*  shaderResourceView,
        ID3D11UnorderedAccessView* unorderedAccessView)
        : FRhiBuffer(desc) {
#if AE_PLATFORM_WIN
        mState = MakeUnique<FState>();
        if (mState) {
            if (buffer) {
                mState->mBuffer.Attach(buffer);
            }
            if (shaderResourceView) {
                mState->mSrv.Attach(shaderResourceView);
            }
            if (unorderedAccessView) {
                mState->mUav.Attach(unorderedAccessView);
            }
        }
#else
        (void)buffer;
        (void)shaderResourceView;
        (void)unorderedAccessView;
#endif
    }

    FRhiD3D11Buffer::FRhiD3D11Buffer(const FRhiBufferDesc& desc) : FRhiBuffer(desc) {
#if AE_PLATFORM_WIN
        mState = MakeUnique<FState>();
#endif
    }

    FRhiD3D11Buffer::~FRhiD3D11Buffer() {
#if AE_PLATFORM_WIN
        mState.Reset();
#endif
    }

    auto FRhiD3D11Buffer::Lock(u64 offset, u64 size, ERhiBufferLockMode mode) -> FLockResult {
        FLockResult lock{};
        lock.mOffset = offset;
        lock.mMode   = mode;

#if AE_PLATFORM_WIN
        const auto& desc = GetDesc();
        if (desc.mSizeBytes == 0ULL) {
            return lock;
        }

        if (size == 0ULL) {
            if (offset >= desc.mSizeBytes) {
                return lock;
            }
            size = desc.mSizeBytes - offset;
        }

        if (offset > desc.mSizeBytes || size > (desc.mSizeBytes - offset)) {
            return lock;
        }

        lock.mSize = size;

        auto* device = static_cast<FRhiD3D11Device*>(RHIGetDevice());
        if (device == nullptr) {
            return {};
        }

        ID3D11DeviceContext* context      = device->GetImmediateContext();
        ID3D11Buffer*        nativeBuffer = GetNativeBuffer();
        if (context == nullptr || nativeBuffer == nullptr) {
            return {};
        }

        const bool cpuRead   = HasAnyFlags(desc.mCpuAccess, ERhiCpuAccess::Read);
        const bool cpuWrite  = HasAnyFlags(desc.mCpuAccess, ERhiCpuAccess::Write);
        const bool isDynamic = desc.mUsage == ERhiResourceUsage::Dynamic;
        const bool isStaging = desc.mUsage == ERhiResourceUsage::Staging;

        if (isStaging) {
            const bool wantsRead =
                (mode == ERhiBufferLockMode::Read) || (mode == ERhiBufferLockMode::ReadWrite);
            const bool wantsWrite = (mode == ERhiBufferLockMode::Write)
                || (mode == ERhiBufferLockMode::WriteDiscard)
                || (mode == ERhiBufferLockMode::WriteNoOverwrite)
                || (mode == ERhiBufferLockMode::ReadWrite);
            if ((wantsRead && !cpuRead) || (wantsWrite && !cpuWrite)) {
                return {};
            }

            D3D11_MAPPED_SUBRESOURCE mapped = {};
            const HRESULT hr = context->Map(nativeBuffer, 0U, ToD3D11Map(mode), 0U, &mapped);
            if (FAILED(hr) || mapped.pData == nullptr) {
                return {};
            }

            lock.mData = static_cast<u8*>(mapped.pData) + offset;

            auto* state          = new FD3D11BufferLockState{};
            state->mPath         = ED3D11BufferLockPath::DirectMap;
            state->mDevice       = device;
            state->mContext      = context;
            state->mNativeBuffer = nativeBuffer;
            lock.mHandle         = state;
            return lock;
        }

        const bool wantsRead =
            (mode == ERhiBufferLockMode::Read) || (mode == ERhiBufferLockMode::ReadWrite);
        const bool wantsWrite = (mode == ERhiBufferLockMode::Write)
            || (mode == ERhiBufferLockMode::WriteDiscard)
            || (mode == ERhiBufferLockMode::WriteNoOverwrite)
            || (mode == ERhiBufferLockMode::ReadWrite);

        if (isDynamic && cpuWrite && wantsWrite && !wantsRead) {
            const D3D11_MAP          mapMode = (mode == ERhiBufferLockMode::WriteDiscard)
                         ? D3D11_MAP_WRITE_DISCARD
                         : D3D11_MAP_WRITE_NO_OVERWRITE;
            D3D11_MAPPED_SUBRESOURCE mapped  = {};
            const HRESULT            hr      = context->Map(nativeBuffer, 0U, mapMode, 0U, &mapped);
            if (FAILED(hr) || mapped.pData == nullptr) {
                return {};
            }
            lock.mData = static_cast<u8*>(mapped.pData) + offset;

            auto* state          = new FD3D11BufferLockState{};
            state->mPath         = ED3D11BufferLockPath::DirectMap;
            state->mDevice       = device;
            state->mContext      = context;
            state->mNativeBuffer = nativeBuffer;
            lock.mHandle         = state;
            return lock;
        }

        if (wantsRead) {
            auto* stagingManager = device->GetStagingBufferManager();
            if (stagingManager == nullptr) {
                return {};
            }
            const ERhiCpuAccess access =
                wantsWrite ? (ERhiCpuAccess::Read | ERhiCpuAccess::Write) : ERhiCpuAccess::Read;
            auto staging = stagingManager->Acquire(size, access);
            if (!staging.IsValid()) {
                return {};
            }

            auto*         stagingBuffer = static_cast<FRhiD3D11Buffer*>(staging.mBuffer);
            ID3D11Buffer* stagingNative =
                stagingBuffer ? stagingBuffer->GetNativeBuffer() : nullptr;
            if (stagingNative == nullptr) {
                stagingManager->Release(staging);
                return {};
            }

            D3D11_BOX box = {};
            if (!BuildBufferBox(offset, size, box)) {
                stagingManager->Release(staging);
                return {};
            }
            context->CopySubresourceRegion(stagingNative, 0U, 0U, 0U, 0U, nativeBuffer, 0U, &box);

            void* mappedData = stagingManager->Map(
                staging, wantsWrite ? ED3D11StagingMapMode::ReadWrite : ED3D11StagingMapMode::Read);
            if (mappedData == nullptr) {
                stagingManager->Release(staging);
                return {};
            }

            lock.mData = mappedData;

            auto* state          = new FD3D11BufferLockState{};
            state->mPath         = ED3D11BufferLockPath::Staging;
            state->mDevice       = device;
            state->mContext      = context;
            state->mNativeBuffer = nativeBuffer;
            state->mStaging      = staging;
            lock.mHandle         = state;
            return lock;
        }

        if (wantsWrite) {
            auto* uploadManager = device->GetUploadBufferManager();
            if (uploadManager == nullptr) {
                return {};
            }
            auto upload = uploadManager->Allocate(size, 16ULL, 0ULL);
            if (!upload.IsValid()) {
                return {};
            }
            void* writePtr = uploadManager->GetWritePointer(upload, 0ULL);
            if (writePtr == nullptr) {
                return {};
            }
            lock.mData = writePtr;

            auto* state          = new FD3D11BufferLockState{};
            state->mPath         = ED3D11BufferLockPath::Upload;
            state->mDevice       = device;
            state->mContext      = context;
            state->mNativeBuffer = nativeBuffer;
            state->mUpload       = upload;
            lock.mHandle         = state;
            return lock;
        }
#else
        (void)offset;
        (void)size;
        (void)mode;
#endif

        return lock;
    }

    void FRhiD3D11Buffer::Unlock(FLockResult& lock) {
#if AE_PLATFORM_WIN
        auto* state = static_cast<FD3D11BufferLockState*>(lock.mHandle);
        if (state == nullptr) {
            lock = {};
            return;
        }

        switch (state->mPath) {
            case ED3D11BufferLockPath::DirectMap:
            {
                if (state->mContext && state->mNativeBuffer) {
                    state->mContext->Unmap(state->mNativeBuffer, 0U);
                }
                break;
            }
            case ED3D11BufferLockPath::Upload:
            {
                if (state->mContext && state->mNativeBuffer && lock.mData && lock.mSize > 0ULL) {
                    D3D11_BOX box = {};
                    if (BuildBufferBox(lock.mOffset, lock.mSize, box)) {
                        state->mContext->UpdateSubresource(
                            state->mNativeBuffer, 0U, &box, lock.mData, 0U, 0U);
                    }
                }
                break;
            }
            case ED3D11BufferLockPath::Staging:
            {
                auto* stagingManager =
                    state->mDevice ? state->mDevice->GetStagingBufferManager() : nullptr;
                if (stagingManager && state->mNativeBuffer) {
                    if (lock.mMode == ERhiBufferLockMode::ReadWrite && lock.mData
                        && lock.mSize > 0ULL) {
                        D3D11_BOX box = {};
                        if (BuildBufferBox(lock.mOffset, lock.mSize, box)) {
                            state->mContext->UpdateSubresource(
                                state->mNativeBuffer, 0U, &box, lock.mData, 0U, 0U);
                        }
                    }
                    stagingManager->Unmap(state->mStaging);
                    stagingManager->Release(state->mStaging);
                }
                break;
            }
            case ED3D11BufferLockPath::None:
            default:
                break;
        }

        delete state;
#else
        (void)lock;
#endif
        lock = {};
    }

    auto FRhiD3D11Buffer::GetNativeBuffer() const noexcept -> ID3D11Buffer* {
#if AE_PLATFORM_WIN
        return mState ? mState->mBuffer.Get() : nullptr;
#else
        return nullptr;
#endif
    }

    auto FRhiD3D11Buffer::GetShaderResourceView() const noexcept -> ID3D11ShaderResourceView* {
#if AE_PLATFORM_WIN
        return mState ? mState->mSrv.Get() : nullptr;
#else
        return nullptr;
#endif
    }

    auto FRhiD3D11Buffer::GetUnorderedAccessView() const noexcept -> ID3D11UnorderedAccessView* {
#if AE_PLATFORM_WIN
        return mState ? mState->mUav.Get() : nullptr;
#else
        return nullptr;
#endif
    }

    FRhiD3D11Texture::FRhiD3D11Texture(const FRhiTextureDesc& desc, ID3D11Resource* resource,
        ID3D11RenderTargetView* renderTargetView, ID3D11DepthStencilView* depthStencilView,
        ID3D11ShaderResourceView*  shaderResourceView,
        ID3D11UnorderedAccessView* unorderedAccessView)
        : FRhiTexture(desc) {
#if AE_PLATFORM_WIN
        mState = MakeUnique<FState>();
        if (mState) {
            if (resource) {
                mState->mResource.Attach(resource);
            }
            if (renderTargetView) {
                mState->mRtv.Attach(renderTargetView);
            }
            if (depthStencilView) {
                mState->mDsv.Attach(depthStencilView);
            }
            if (shaderResourceView) {
                mState->mSrv.Attach(shaderResourceView);
            }
            if (unorderedAccessView) {
                mState->mUav.Attach(unorderedAccessView);
            }
        }
#else
        (void)resource;
        (void)renderTargetView;
        (void)depthStencilView;
        (void)shaderResourceView;
        (void)unorderedAccessView;
#endif
    }

    FRhiD3D11Texture::FRhiD3D11Texture(const FRhiTextureDesc& desc) : FRhiTexture(desc) {
#if AE_PLATFORM_WIN
        mState = MakeUnique<FState>();
#endif
    }

    FRhiD3D11Texture::~FRhiD3D11Texture() {
#if AE_PLATFORM_WIN
        mState.Reset();
#endif
    }

    auto FRhiD3D11Texture::GetNativeResource() const noexcept -> ID3D11Resource* {
#if AE_PLATFORM_WIN
        return mState ? mState->mResource.Get() : nullptr;
#else
        return nullptr;
#endif
    }

    auto FRhiD3D11Texture::GetRenderTargetView() const noexcept -> ID3D11RenderTargetView* {
#if AE_PLATFORM_WIN
        return mState ? mState->mRtv.Get() : nullptr;
#else
        return nullptr;
#endif
    }

    auto FRhiD3D11Texture::GetDepthStencilView() const noexcept -> ID3D11DepthStencilView* {
#if AE_PLATFORM_WIN
        return mState ? mState->mDsv.Get() : nullptr;
#else
        return nullptr;
#endif
    }

    auto FRhiD3D11Texture::GetShaderResourceView() const noexcept -> ID3D11ShaderResourceView* {
#if AE_PLATFORM_WIN
        return mState ? mState->mSrv.Get() : nullptr;
#else
        return nullptr;
#endif
    }

    auto FRhiD3D11Texture::GetUnorderedAccessView() const noexcept -> ID3D11UnorderedAccessView* {
#if AE_PLATFORM_WIN
        return mState ? mState->mUav.Get() : nullptr;
#else
        return nullptr;
#endif
    }

    FRhiD3D11Sampler::FRhiD3D11Sampler(const FRhiSamplerDesc& desc, ID3D11SamplerState* sampler)
        : FRhiSampler(desc) {
#if AE_PLATFORM_WIN
        mState = MakeUnique<FState>();
        if (mState && sampler) {
            mState->mSampler.Attach(sampler);
        }
#else
        (void)sampler;
#endif
    }

    FRhiD3D11Sampler::FRhiD3D11Sampler(const FRhiSamplerDesc& desc) : FRhiSampler(desc) {
#if AE_PLATFORM_WIN
        mState = MakeUnique<FState>();
#endif
    }

    FRhiD3D11Sampler::~FRhiD3D11Sampler() {
#if AE_PLATFORM_WIN
        mState.Reset();
#endif
    }

    auto FRhiD3D11Sampler::GetNativeSampler() const noexcept -> ID3D11SamplerState* {
#if AE_PLATFORM_WIN
        return mState ? mState->mSampler.Get() : nullptr;
#else
        return nullptr;
#endif
    }

    namespace {
#if AE_PLATFORM_WIN
        [[nodiscard]] auto ToD3D11Map(ERhiBufferLockMode mode) noexcept -> D3D11_MAP {
            switch (mode) {
                case ERhiBufferLockMode::WriteDiscard:
                    return D3D11_MAP_WRITE_DISCARD;
                case ERhiBufferLockMode::WriteNoOverwrite:
                    return D3D11_MAP_WRITE_NO_OVERWRITE;
                case ERhiBufferLockMode::Write:
                    return D3D11_MAP_WRITE_NO_OVERWRITE;
                case ERhiBufferLockMode::ReadWrite:
                    return D3D11_MAP_READ_WRITE;
                case ERhiBufferLockMode::Read:
                default:
                    return D3D11_MAP_READ;
            }
        }

        [[nodiscard]] auto BuildBufferBox(u64 offset, u64 size, D3D11_BOX& box) noexcept -> bool {
            const u64 end     = offset + size;
            const u64 maxUint = static_cast<u64>(std::numeric_limits<UINT>::max());
            if (offset > maxUint || end > maxUint) {
                return false;
            }
            box.left   = static_cast<UINT>(offset);
            box.right  = static_cast<UINT>(end);
            box.top    = 0U;
            box.bottom = 1U;
            box.front  = 0U;
            box.back   = 1U;
            return true;
        }

        auto ToD3D11Usage(ERhiResourceUsage usage) noexcept -> D3D11_USAGE {
            switch (usage) {
                case ERhiResourceUsage::Immutable:
                    return D3D11_USAGE_IMMUTABLE;
                case ERhiResourceUsage::Dynamic:
                    return D3D11_USAGE_DYNAMIC;
                case ERhiResourceUsage::Staging:
                    return D3D11_USAGE_STAGING;
                case ERhiResourceUsage::Default:
                default:
                    return D3D11_USAGE_DEFAULT;
            }
        }

        auto ToD3D11CpuAccess(ERhiCpuAccess access) noexcept -> UINT {
            UINT flags = 0U;
            if (HasAnyFlags(access, ERhiCpuAccess::Read)) {
                flags |= D3D11_CPU_ACCESS_READ;
            }
            if (HasAnyFlags(access, ERhiCpuAccess::Write)) {
                flags |= D3D11_CPU_ACCESS_WRITE;
            }
            return flags;
        }

        auto ToD3D11BufferBindFlags(ERhiBufferBindFlags flags) noexcept -> UINT {
            UINT result = 0U;
            if (HasAnyFlags(flags, ERhiBufferBindFlags::Vertex)) {
                result |= D3D11_BIND_VERTEX_BUFFER;
            }
            if (HasAnyFlags(flags, ERhiBufferBindFlags::Index)) {
                result |= D3D11_BIND_INDEX_BUFFER;
            }
            if (HasAnyFlags(flags, ERhiBufferBindFlags::Constant)) {
                result |= D3D11_BIND_CONSTANT_BUFFER;
            }
            if (HasAnyFlags(flags, ERhiBufferBindFlags::ShaderResource)) {
                result |= D3D11_BIND_SHADER_RESOURCE;
            }
            if (HasAnyFlags(flags, ERhiBufferBindFlags::UnorderedAccess)) {
                result |= D3D11_BIND_UNORDERED_ACCESS;
            }
            return result;
        }

        auto ToD3D11TextureBindFlags(ERhiTextureBindFlags flags) noexcept -> UINT {
            UINT result = 0U;
            if (HasAnyFlags(flags, ERhiTextureBindFlags::ShaderResource)) {
                result |= D3D11_BIND_SHADER_RESOURCE;
            }
            if (HasAnyFlags(flags, ERhiTextureBindFlags::RenderTarget)) {
                result |= D3D11_BIND_RENDER_TARGET;
            }
            if (HasAnyFlags(flags, ERhiTextureBindFlags::DepthStencil)) {
                result |= D3D11_BIND_DEPTH_STENCIL;
            }
            if (HasAnyFlags(flags, ERhiTextureBindFlags::UnorderedAccess)) {
                result |= D3D11_BIND_UNORDERED_ACCESS;
            }
            return result;
        }

        auto ToD3D11Format(ERhiFormat format) noexcept -> DXGI_FORMAT {
            switch (format) {
                case ERhiFormat::R8G8B8A8Unorm:
                    return DXGI_FORMAT_R8G8B8A8_UNORM;
                case ERhiFormat::R8G8B8A8UnormSrgb:
                    return DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
                case ERhiFormat::B8G8R8A8Unorm:
                    return DXGI_FORMAT_B8G8R8A8_UNORM;
                case ERhiFormat::B8G8R8A8UnormSrgb:
                    return DXGI_FORMAT_B8G8R8A8_UNORM_SRGB;
                case ERhiFormat::R16G16B16A16Float:
                    return DXGI_FORMAT_R16G16B16A16_FLOAT;
                case ERhiFormat::R32G32B32Float:
                    return DXGI_FORMAT_R32G32B32_FLOAT;
                case ERhiFormat::R32Float:
                    return DXGI_FORMAT_R32_FLOAT;
                case ERhiFormat::D24UnormS8Uint:
                    return DXGI_FORMAT_D24_UNORM_S8_UINT;
                case ERhiFormat::D32Float:
                    return DXGI_FORMAT_D32_FLOAT;
                case ERhiFormat::Unknown:
                default:
                    return DXGI_FORMAT_UNKNOWN;
            }
        }

        auto IsDepthStencilFormat(ERhiFormat format) noexcept -> bool {
            switch (format) {
                case ERhiFormat::D24UnormS8Uint:
                case ERhiFormat::D32Float:
                    return true;
                default:
                    return false;
            }
        }

        auto CreateTextureRtv(ID3D11Device* device, ID3D11Resource* resource,
            const FRhiTextureDesc& desc) -> ComPtr<ID3D11RenderTargetView> {
            ComPtr<ID3D11RenderTargetView> rtv;
            if (device == nullptr || resource == nullptr) {
                return rtv;
            }
            if (IsDepthStencilFormat(desc.mFormat)) {
                return rtv;
            }

            const DXGI_FORMAT format = ToD3D11Format(desc.mFormat);
            if (format == DXGI_FORMAT_UNKNOWN) {
                return rtv;
            }

            D3D11_RENDER_TARGET_VIEW_DESC viewDesc = {};
            viewDesc.Format                        = format;

            if (desc.mDepth > 1U) {
                viewDesc.ViewDimension         = D3D11_RTV_DIMENSION_TEXTURE3D;
                viewDesc.Texture3D.MipSlice    = 0U;
                viewDesc.Texture3D.FirstWSlice = 0U;
                viewDesc.Texture3D.WSize       = desc.mDepth;
            } else if (desc.mSampleCount > 1U) {
                if (desc.mArrayLayers > 1U) {
                    viewDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2DMSARRAY;
                    viewDesc.Texture2DMSArray.FirstArraySlice = 0U;
                    viewDesc.Texture2DMSArray.ArraySize       = desc.mArrayLayers;
                } else {
                    viewDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2DMS;
                }
            } else if (desc.mArrayLayers > 1U) {
                viewDesc.ViewDimension                  = D3D11_RTV_DIMENSION_TEXTURE2DARRAY;
                viewDesc.Texture2DArray.MipSlice        = 0U;
                viewDesc.Texture2DArray.FirstArraySlice = 0U;
                viewDesc.Texture2DArray.ArraySize       = desc.mArrayLayers;
            } else {
                viewDesc.ViewDimension      = D3D11_RTV_DIMENSION_TEXTURE2D;
                viewDesc.Texture2D.MipSlice = 0U;
            }

            const HRESULT hr = device->CreateRenderTargetView(resource, &viewDesc, &rtv);
            if (FAILED(hr)) {
                rtv.Reset();
            }
            return rtv;
        }

        auto CreateTextureDsv(ID3D11Device* device, ID3D11Resource* resource,
            const FRhiTextureDesc& desc) -> ComPtr<ID3D11DepthStencilView> {
            ComPtr<ID3D11DepthStencilView> dsv;
            if (device == nullptr || resource == nullptr) {
                return dsv;
            }
            if (!IsDepthStencilFormat(desc.mFormat)) {
                return dsv;
            }
            if (desc.mDepth > 1U) {
                return dsv;
            }

            const DXGI_FORMAT format = ToD3D11Format(desc.mFormat);
            if (format == DXGI_FORMAT_UNKNOWN) {
                return dsv;
            }

            D3D11_DEPTH_STENCIL_VIEW_DESC viewDesc = {};
            viewDesc.Format                        = format;

            if (desc.mSampleCount > 1U) {
                if (desc.mArrayLayers > 1U) {
                    viewDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2DMSARRAY;
                    viewDesc.Texture2DMSArray.FirstArraySlice = 0U;
                    viewDesc.Texture2DMSArray.ArraySize       = desc.mArrayLayers;
                } else {
                    viewDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2DMS;
                }
            } else if (desc.mArrayLayers > 1U) {
                viewDesc.ViewDimension                  = D3D11_DSV_DIMENSION_TEXTURE2DARRAY;
                viewDesc.Texture2DArray.MipSlice        = 0U;
                viewDesc.Texture2DArray.FirstArraySlice = 0U;
                viewDesc.Texture2DArray.ArraySize       = desc.mArrayLayers;
            } else {
                viewDesc.ViewDimension      = D3D11_DSV_DIMENSION_TEXTURE2D;
                viewDesc.Texture2D.MipSlice = 0U;
            }

            const HRESULT hr = device->CreateDepthStencilView(resource, &viewDesc, &dsv);
            if (FAILED(hr)) {
                dsv.Reset();
            }
            return dsv;
        }

        auto CreateBufferSrv(ID3D11Device* device, ID3D11Buffer* buffer, const FRhiBufferDesc& desc)
            -> ComPtr<ID3D11ShaderResourceView> {
            ComPtr<ID3D11ShaderResourceView> srv;
            if (device == nullptr || buffer == nullptr) {
                return srv;
            }
            if ((desc.mSizeBytes == 0ULL) || (desc.mSizeBytes % 4ULL != 0ULL)) {
                return srv;
            }
            const UINT elementCount = static_cast<UINT>(desc.mSizeBytes / 4ULL);
            if (elementCount == 0U) {
                return srv;
            }
            D3D11_SHADER_RESOURCE_VIEW_DESC viewDesc = {};
            viewDesc.ViewDimension                   = D3D11_SRV_DIMENSION_BUFFEREX;
            viewDesc.Format                          = DXGI_FORMAT_R32_TYPELESS;
            viewDesc.BufferEx.FirstElement           = 0U;
            viewDesc.BufferEx.NumElements            = elementCount;
            viewDesc.BufferEx.Flags                  = D3D11_BUFFEREX_SRV_FLAG_RAW;
            const HRESULT hr = device->CreateShaderResourceView(buffer, &viewDesc, &srv);
            if (FAILED(hr)) {
                srv.Reset();
            }
            return srv;
        }

        auto CreateBufferUav(ID3D11Device* device, ID3D11Buffer* buffer, const FRhiBufferDesc& desc)
            -> ComPtr<ID3D11UnorderedAccessView> {
            ComPtr<ID3D11UnorderedAccessView> uav;
            if (device == nullptr || buffer == nullptr) {
                return uav;
            }
            if ((desc.mSizeBytes == 0ULL) || (desc.mSizeBytes % 4ULL != 0ULL)) {
                return uav;
            }
            const UINT elementCount = static_cast<UINT>(desc.mSizeBytes / 4ULL);
            if (elementCount == 0U) {
                return uav;
            }
            D3D11_UNORDERED_ACCESS_VIEW_DESC viewDesc = {};
            viewDesc.ViewDimension                    = D3D11_UAV_DIMENSION_BUFFER;
            viewDesc.Format                           = DXGI_FORMAT_R32_TYPELESS;
            viewDesc.Buffer.FirstElement              = 0U;
            viewDesc.Buffer.NumElements               = elementCount;
            viewDesc.Buffer.Flags                     = D3D11_BUFFER_UAV_FLAG_RAW;
            const HRESULT hr = device->CreateUnorderedAccessView(buffer, &viewDesc, &uav);
            if (FAILED(hr)) {
                uav.Reset();
            }
            return uav;
        }

        auto CreateTextureSrv(ID3D11Device* device, ID3D11Resource* resource,
            const FRhiTextureDesc& desc) -> ComPtr<ID3D11ShaderResourceView> {
            ComPtr<ID3D11ShaderResourceView> srv;
            if (device == nullptr || resource == nullptr) {
                return srv;
            }
            const DXGI_FORMAT format = ToD3D11Format(desc.mFormat);
            if (format == DXGI_FORMAT_UNKNOWN) {
                return srv;
            }

            D3D11_SHADER_RESOURCE_VIEW_DESC viewDesc = {};
            viewDesc.Format                          = format;

            if (desc.mDepth > 1U) {
                viewDesc.ViewDimension             = D3D11_SRV_DIMENSION_TEXTURE3D;
                viewDesc.Texture3D.MostDetailedMip = 0U;
                viewDesc.Texture3D.MipLevels       = desc.mMipLevels;
            } else if (desc.mSampleCount > 1U) {
                if (desc.mArrayLayers > 1U) {
                    viewDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2DMSARRAY;
                    viewDesc.Texture2DMSArray.FirstArraySlice = 0U;
                    viewDesc.Texture2DMSArray.ArraySize       = desc.mArrayLayers;
                } else {
                    viewDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2DMS;
                }
            } else if (desc.mArrayLayers > 1U) {
                viewDesc.ViewDimension                  = D3D11_SRV_DIMENSION_TEXTURE2DARRAY;
                viewDesc.Texture2DArray.MostDetailedMip = 0U;
                viewDesc.Texture2DArray.MipLevels       = desc.mMipLevels;
                viewDesc.Texture2DArray.FirstArraySlice = 0U;
                viewDesc.Texture2DArray.ArraySize       = desc.mArrayLayers;
            } else {
                viewDesc.ViewDimension             = D3D11_SRV_DIMENSION_TEXTURE2D;
                viewDesc.Texture2D.MostDetailedMip = 0U;
                viewDesc.Texture2D.MipLevels       = desc.mMipLevels;
            }

            const HRESULT hr = device->CreateShaderResourceView(resource, &viewDesc, &srv);
            if (FAILED(hr)) {
                srv.Reset();
            }
            return srv;
        }

        auto CreateTextureUav(ID3D11Device* device, ID3D11Resource* resource,
            const FRhiTextureDesc& desc) -> ComPtr<ID3D11UnorderedAccessView> {
            ComPtr<ID3D11UnorderedAccessView> uav;
            if (device == nullptr || resource == nullptr) {
                return uav;
            }
            if (desc.mSampleCount > 1U) {
                return uav;
            }
            const DXGI_FORMAT format = ToD3D11Format(desc.mFormat);
            if (format == DXGI_FORMAT_UNKNOWN) {
                return uav;
            }

            D3D11_UNORDERED_ACCESS_VIEW_DESC viewDesc = {};
            viewDesc.Format                           = format;

            if (desc.mDepth > 1U) {
                viewDesc.ViewDimension         = D3D11_UAV_DIMENSION_TEXTURE3D;
                viewDesc.Texture3D.MipSlice    = 0U;
                viewDesc.Texture3D.FirstWSlice = 0U;
                viewDesc.Texture3D.WSize       = desc.mDepth;
            } else if (desc.mArrayLayers > 1U) {
                viewDesc.ViewDimension                  = D3D11_UAV_DIMENSION_TEXTURE2DARRAY;
                viewDesc.Texture2DArray.MipSlice        = 0U;
                viewDesc.Texture2DArray.FirstArraySlice = 0U;
                viewDesc.Texture2DArray.ArraySize       = desc.mArrayLayers;
            } else {
                viewDesc.ViewDimension      = D3D11_UAV_DIMENSION_TEXTURE2D;
                viewDesc.Texture2D.MipSlice = 0U;
            }

            const HRESULT hr = device->CreateUnorderedAccessView(resource, &viewDesc, &uav);
            if (FAILED(hr)) {
                uav.Reset();
            }
            return uav;
        }
#endif
    } // namespace

    auto FRhiD3D11Device::CreateBuffer(const FRhiBufferDesc& desc) -> FRhiBufferRef {
#if AE_PLATFORM_WIN
        ID3D11Device* device = GetNativeDevice();
        if ((device == nullptr) || desc.mSizeBytes == 0ULL) {
            return {};
        }

        if (desc.mSizeBytes > static_cast<u64>(std::numeric_limits<UINT>::max())) {
            return {};
        }

        const D3D11_USAGE usage     = ToD3D11Usage(desc.mUsage);
        UINT              bindFlags = ToD3D11BufferBindFlags(desc.mBindFlags);
        const UINT        cpuAccess = ToD3D11CpuAccess(desc.mCpuAccess);
        UINT              miscFlags = 0U;

        if (HasAnyFlags(desc.mBindFlags, ERhiBufferBindFlags::Indirect)) {
            miscFlags |= D3D11_RESOURCE_MISC_DRAWINDIRECT_ARGS;
        }
        const bool wantsSrv = HasAnyFlags(desc.mBindFlags, ERhiBufferBindFlags::ShaderResource);
        const bool wantsUav = HasAnyFlags(desc.mBindFlags, ERhiBufferBindFlags::UnorderedAccess);
        if (wantsSrv || wantsUav) {
            miscFlags |= D3D11_RESOURCE_MISC_BUFFER_ALLOW_RAW_VIEWS;
        }

        if (usage == D3D11_USAGE_IMMUTABLE && cpuAccess != 0U) {
            return {};
        }
        if (usage == D3D11_USAGE_DYNAMIC && (cpuAccess & D3D11_CPU_ACCESS_WRITE) == 0U) {
            return {};
        }
        if (usage == D3D11_USAGE_STAGING) {
            if (bindFlags != 0U) {
                return {};
            }
            if (cpuAccess == 0U) {
                return {};
            }
        }

        D3D11_BUFFER_DESC bufferDesc   = {};
        bufferDesc.ByteWidth           = static_cast<UINT>(desc.mSizeBytes);
        bufferDesc.Usage               = usage;
        bufferDesc.BindFlags           = bindFlags;
        bufferDesc.CPUAccessFlags      = cpuAccess;
        bufferDesc.MiscFlags           = miscFlags;
        bufferDesc.StructureByteStride = 0U;

        ComPtr<ID3D11Buffer> buffer;
        const HRESULT        hr = device->CreateBuffer(&bufferDesc, nullptr, &buffer);
        if (FAILED(hr) || (buffer == nullptr)) {
            return {};
        }

        ComPtr<ID3D11ShaderResourceView>  srv;
        ComPtr<ID3D11UnorderedAccessView> uav;
        if (wantsSrv) {
            srv = CreateBufferSrv(device, buffer.Get(), desc);
        }
        if (wantsUav) {
            uav = CreateBufferUav(device, buffer.Get(), desc);
        }

        return MakeResource<FRhiD3D11Buffer>(desc, buffer.Detach(), srv.Detach(), uav.Detach());
#else
        return MakeResource<FRhiD3D11Buffer>(desc);
#endif
    }

    auto FRhiD3D11Device::CreateTexture(const FRhiTextureDesc& desc) -> FRhiTextureRef {
#if AE_PLATFORM_WIN
        ID3D11Device* device = GetNativeDevice();
        if (!device) {
            return {};
        }

        if (desc.mWidth == 0U || desc.mHeight == 0U || desc.mMipLevels == 0U) {
            return {};
        }

        if (desc.mArrayLayers == 0U) {
            return {};
        }

        const DXGI_FORMAT format = ToD3D11Format(desc.mFormat);
        if (format == DXGI_FORMAT_UNKNOWN) {
            return {};
        }

        const D3D11_USAGE usage     = ToD3D11Usage(desc.mUsage);
        UINT              bindFlags = ToD3D11TextureBindFlags(desc.mBindFlags);
        const UINT        cpuAccess = ToD3D11CpuAccess(desc.mCpuAccess);

        if (usage == D3D11_USAGE_IMMUTABLE && cpuAccess != 0U) {
            return {};
        }
        if (usage == D3D11_USAGE_DYNAMIC && (cpuAccess & D3D11_CPU_ACCESS_WRITE) == 0U) {
            return {};
        }
        if (usage == D3D11_USAGE_STAGING) {
            if (bindFlags != 0U) {
                return {};
            }
            if (cpuAccess == 0U) {
                return {};
            }
        }

        if (desc.mDepth > 1U) {
            if (desc.mArrayLayers > 1U) {
                return {};
            }
            if (desc.mDepth > static_cast<u32>(std::numeric_limits<UINT>::max())) {
                return {};
            }

            D3D11_TEXTURE3D_DESC texDesc = {};
            texDesc.Width                = desc.mWidth;
            texDesc.Height               = desc.mHeight;
            texDesc.Depth                = desc.mDepth;
            texDesc.MipLevels            = desc.mMipLevels;
            texDesc.Format               = format;
            texDesc.Usage                = usage;
            texDesc.BindFlags            = bindFlags;
            texDesc.CPUAccessFlags       = cpuAccess;

            ComPtr<ID3D11Texture3D> texture;
            const HRESULT           hr = device->CreateTexture3D(&texDesc, nullptr, &texture);
            if (FAILED(hr) || (texture == nullptr)) {
                return {};
            }

            ComPtr<ID3D11Resource> resource;
            texture.As(&resource);
            ComPtr<ID3D11ShaderResourceView>  srv;
            ComPtr<ID3D11UnorderedAccessView> uav;
            ComPtr<ID3D11RenderTargetView>    rtv;
            ComPtr<ID3D11DepthStencilView>    dsv;
            if (HasAnyFlags(desc.mBindFlags, ERhiTextureBindFlags::ShaderResource)) {
                srv = CreateTextureSrv(device, resource.Get(), desc);
            }
            if (HasAnyFlags(desc.mBindFlags, ERhiTextureBindFlags::UnorderedAccess)) {
                uav = CreateTextureUav(device, resource.Get(), desc);
            }
            if (HasAnyFlags(desc.mBindFlags, ERhiTextureBindFlags::RenderTarget)) {
                rtv = CreateTextureRtv(device, resource.Get(), desc);
            }
            if (HasAnyFlags(desc.mBindFlags, ERhiTextureBindFlags::DepthStencil)) {
                dsv = CreateTextureDsv(device, resource.Get(), desc);
            }
            return MakeResource<FRhiD3D11Texture>(
                desc, resource.Detach(), rtv.Detach(), dsv.Detach(), srv.Detach(), uav.Detach());
        }

        if (desc.mSampleCount == 0U) {
            return {};
        }
        if (desc.mSampleCount > 1U && desc.mMipLevels > 1U) {
            return {};
        }
        if (desc.mArrayLayers > static_cast<u32>(std::numeric_limits<UINT>::max())) {
            return {};
        }

        D3D11_TEXTURE2D_DESC texDesc = {};
        texDesc.Width                = desc.mWidth;
        texDesc.Height               = desc.mHeight;
        texDesc.MipLevels            = desc.mMipLevels;
        texDesc.ArraySize            = desc.mArrayLayers;
        texDesc.Format               = format;
        texDesc.SampleDesc.Count     = desc.mSampleCount;
        texDesc.SampleDesc.Quality   = 0U;
        texDesc.Usage                = usage;
        texDesc.BindFlags            = bindFlags;
        texDesc.CPUAccessFlags       = cpuAccess;

        ComPtr<ID3D11Texture2D> texture;
        const HRESULT           hr = device->CreateTexture2D(&texDesc, nullptr, &texture);
        if (FAILED(hr) || (texture == nullptr)) {
            return {};
        }

        ComPtr<ID3D11Resource> resource;
        texture.As(&resource);
        ComPtr<ID3D11ShaderResourceView>  srv;
        ComPtr<ID3D11UnorderedAccessView> uav;
        ComPtr<ID3D11RenderTargetView>    rtv;
        ComPtr<ID3D11DepthStencilView>    dsv;
        if (HasAnyFlags(desc.mBindFlags, ERhiTextureBindFlags::ShaderResource)) {
            srv = CreateTextureSrv(device, resource.Get(), desc);
        }
        if (HasAnyFlags(desc.mBindFlags, ERhiTextureBindFlags::UnorderedAccess)) {
            uav = CreateTextureUav(device, resource.Get(), desc);
        }
        if (HasAnyFlags(desc.mBindFlags, ERhiTextureBindFlags::RenderTarget)) {
            rtv = CreateTextureRtv(device, resource.Get(), desc);
        }
        if (HasAnyFlags(desc.mBindFlags, ERhiTextureBindFlags::DepthStencil)) {
            dsv = CreateTextureDsv(device, resource.Get(), desc);
        }
        return MakeResource<FRhiD3D11Texture>(
            desc, resource.Detach(), rtv.Detach(), dsv.Detach(), srv.Detach(), uav.Detach());
#else
        return MakeResource<FRhiD3D11Texture>(desc);
#endif
    }
} // namespace AltinaEngine::Rhi
