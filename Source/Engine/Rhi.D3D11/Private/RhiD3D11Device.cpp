#include "RhiD3D11/RhiD3D11Device.h"

#include "Rhi/RhiBindGroup.h"
#include "Rhi/RhiBindGroupLayout.h"
#include "Rhi/RhiBuffer.h"
#include "Rhi/RhiCommandPool.h"
#include "Rhi/RhiFence.h"
#include "Rhi/RhiPipeline.h"
#include "Rhi/RhiPipelineLayout.h"
#include "Rhi/RhiQueue.h"
#include "Rhi/RhiSampler.h"
#include "Rhi/RhiSemaphore.h"
#include "Rhi/RhiShader.h"
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
#include <type_traits>

namespace AltinaEngine::Rhi {
#if AE_PLATFORM_WIN
    using Microsoft::WRL::ComPtr;

    struct FRhiD3D11Device::FState {
        ComPtr<ID3D11Device>        mDevice;
        ComPtr<ID3D11DeviceContext> mImmediateContext;
        D3D_FEATURE_LEVEL           mFeatureLevel = D3D_FEATURE_LEVEL_11_0;
    };
#else
    struct FRhiD3D11Device::FState {};
#endif

    namespace {
#if AE_PLATFORM_WIN
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
            case ERhiFormat::R8G8B8A8_UNORM:
                return DXGI_FORMAT_R8G8B8A8_UNORM;
            case ERhiFormat::R8G8B8A8_UNORM_SRGB:
                return DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
            case ERhiFormat::B8G8R8A8_UNORM:
                return DXGI_FORMAT_B8G8R8A8_UNORM;
            case ERhiFormat::B8G8R8A8_UNORM_SRGB:
                return DXGI_FORMAT_B8G8R8A8_UNORM_SRGB;
            case ERhiFormat::R16G16B16A16_FLOAT:
                return DXGI_FORMAT_R16G16B16A16_FLOAT;
            case ERhiFormat::R32_FLOAT:
                return DXGI_FORMAT_R32_FLOAT;
            case ERhiFormat::D24_UNORM_S8_UINT:
                return DXGI_FORMAT_D24_UNORM_S8_UINT;
            case ERhiFormat::D32_FLOAT:
                return DXGI_FORMAT_D32_FLOAT;
            case ERhiFormat::Unknown:
            default:
                return DXGI_FORMAT_UNKNOWN;
            }
        }

        class FRhiD3D11Buffer final : public FRhiBuffer {
        public:
            FRhiD3D11Buffer(const FRhiBufferDesc& desc, ComPtr<ID3D11Buffer> buffer)
                : FRhiBuffer(desc), mBuffer(AltinaEngine::Move(buffer)) {}

        private:
            ComPtr<ID3D11Buffer> mBuffer;
        };

        class FRhiD3D11Texture final : public FRhiTexture {
        public:
            FRhiD3D11Texture(const FRhiTextureDesc& desc, ComPtr<ID3D11Resource> resource)
                : FRhiTexture(desc), mResource(AltinaEngine::Move(resource)) {}

        private:
            ComPtr<ID3D11Resource> mResource;
        };
#else
        class FRhiD3D11Buffer final : public FRhiBuffer {
        public:
            explicit FRhiD3D11Buffer(const FRhiBufferDesc& desc) : FRhiBuffer(desc) {}
        };

        class FRhiD3D11Texture final : public FRhiTexture {
        public:
            explicit FRhiD3D11Texture(const FRhiTextureDesc& desc) : FRhiTexture(desc) {}
        };
#endif

        class FRhiD3D11Sampler final : public FRhiSampler {
        public:
            explicit FRhiD3D11Sampler(const FRhiSamplerDesc& desc) : FRhiSampler(desc) {}
        };

        class FRhiD3D11Shader final : public FRhiShader {
        public:
            explicit FRhiD3D11Shader(const FRhiShaderDesc& desc) : FRhiShader(desc) {}
        };

        class FRhiD3D11GraphicsPipeline final : public FRhiPipeline {
        public:
            explicit FRhiD3D11GraphicsPipeline(const FRhiGraphicsPipelineDesc& desc)
                : FRhiPipeline(desc) {}
        };

        class FRhiD3D11ComputePipeline final : public FRhiPipeline {
        public:
            explicit FRhiD3D11ComputePipeline(const FRhiComputePipelineDesc& desc)
                : FRhiPipeline(desc) {}
        };

        class FRhiD3D11PipelineLayout final : public FRhiPipelineLayout {
        public:
            explicit FRhiD3D11PipelineLayout(const FRhiPipelineLayoutDesc& desc)
                : FRhiPipelineLayout(desc) {}
        };

        class FRhiD3D11BindGroupLayout final : public FRhiBindGroupLayout {
        public:
            explicit FRhiD3D11BindGroupLayout(const FRhiBindGroupLayoutDesc& desc)
                : FRhiBindGroupLayout(desc) {}
        };

        class FRhiD3D11BindGroup final : public FRhiBindGroup {
        public:
            explicit FRhiD3D11BindGroup(const FRhiBindGroupDesc& desc)
                : FRhiBindGroup(desc) {}
        };

        class FRhiD3D11Fence final : public FRhiFence {
        public:
            FRhiD3D11Fence() : FRhiFence() {}
        };

        class FRhiD3D11Semaphore final : public FRhiSemaphore {
        public:
            FRhiD3D11Semaphore() : FRhiSemaphore() {}
        };

        class FRhiD3D11CommandPool final : public FRhiCommandPool {
        public:
            explicit FRhiD3D11CommandPool(const FRhiCommandPoolDesc& desc)
                : FRhiCommandPool(desc) {}
        };

        class FRhiD3D11Queue final : public FRhiQueue {
        public:
            explicit FRhiD3D11Queue(ERhiQueueType type) : FRhiQueue(type) {}

            void Submit(const FRhiSubmitInfo& /*info*/) override {}
            void WaitIdle() override {}
            void Present(const FRhiPresentInfo& /*info*/) override {}
        };
    } // namespace

    FRhiD3D11Device::FRhiD3D11Device(const FRhiDeviceDesc& desc,
        const FRhiAdapterDesc& adapterDesc, ID3D11Device* device,
        ID3D11DeviceContext* context, u32 featureLevel)
        : FRhiDevice(desc, adapterDesc) {
        mState = new FState{};

#if AE_PLATFORM_WIN
        if (mState) {
            mState->mDevice.Attach(device);
            mState->mImmediateContext.Attach(context);
            mState->mFeatureLevel = static_cast<D3D_FEATURE_LEVEL>(featureLevel);
        }

        FRhiSupportedLimits limits;
        limits.mMaxTextureDimension1D     = D3D11_REQ_TEXTURE1D_U_DIMENSION;
        limits.mMaxTextureDimension2D     = D3D11_REQ_TEXTURE2D_U_OR_V_DIMENSION;
        limits.mMaxTextureDimension3D     = D3D11_REQ_TEXTURE3D_U_V_OR_W_DIMENSION;
        limits.mMaxTextureArrayLayers     = D3D11_REQ_TEXTURE2D_ARRAY_AXIS_DIMENSION;
        limits.mMaxSamplers               = D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT;
        limits.mMaxColorAttachments       = D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT;
        SetSupportedLimits(limits);
#else
        (void)device;
        (void)context;
        (void)featureLevel;
#endif

        RegisterQueue(ERhiQueueType::Graphics,
            AdoptResource(new FRhiD3D11Queue(ERhiQueueType::Graphics)));
        RegisterQueue(ERhiQueueType::Compute,
            AdoptResource(new FRhiD3D11Queue(ERhiQueueType::Compute)));
        RegisterQueue(ERhiQueueType::Copy,
            AdoptResource(new FRhiD3D11Queue(ERhiQueueType::Copy)));
    }

    FRhiD3D11Device::~FRhiD3D11Device() {
        delete mState;
        mState = nullptr;
    }

    auto FRhiD3D11Device::GetNativeDevice() const noexcept -> ID3D11Device* {
#if AE_PLATFORM_WIN
        return mState ? mState->mDevice.Get() : nullptr;
#else
        return nullptr;
#endif
    }

    auto FRhiD3D11Device::GetImmediateContext() const noexcept -> ID3D11DeviceContext* {
#if AE_PLATFORM_WIN
        return mState ? mState->mImmediateContext.Get() : nullptr;
#else
        return nullptr;
#endif
    }

    auto FRhiD3D11Device::GetFeatureLevel() const noexcept -> u32 {
#if AE_PLATFORM_WIN
        return mState ? static_cast<u32>(mState->mFeatureLevel) : 0U;
#else
        return 0U;
#endif
    }

    auto FRhiD3D11Device::CreateBuffer(const FRhiBufferDesc& desc) -> FRhiBufferRef {
#if AE_PLATFORM_WIN
        if (!mState || !mState->mDevice || desc.mSizeBytes == 0ULL) {
            return {};
        }

        if (desc.mSizeBytes > static_cast<u64>(std::numeric_limits<UINT>::max())) {
            return {};
        }

        const D3D11_USAGE usage    = ToD3D11Usage(desc.mUsage);
        UINT              bindFlags = ToD3D11BufferBindFlags(desc.mBindFlags);
        const UINT        cpuAccess = ToD3D11CpuAccess(desc.mCpuAccess);
        UINT              miscFlags = 0U;

        if (HasAnyFlags(desc.mBindFlags, ERhiBufferBindFlags::Indirect)) {
            miscFlags |= D3D11_RESOURCE_MISC_DRAWINDIRECT_ARGS;
        }

        if (usage == D3D11_USAGE_IMMUTABLE && cpuAccess != 0U) {
            return {};
        }
        if (usage == D3D11_USAGE_DYNAMIC
            && (cpuAccess & D3D11_CPU_ACCESS_WRITE) == 0U) {
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

        D3D11_BUFFER_DESC bufferDesc = {};
        bufferDesc.ByteWidth      = static_cast<UINT>(desc.mSizeBytes);
        bufferDesc.Usage          = usage;
        bufferDesc.BindFlags      = bindFlags;
        bufferDesc.CPUAccessFlags = cpuAccess;
        bufferDesc.MiscFlags      = miscFlags;
        bufferDesc.StructureByteStride = 0U;

        ComPtr<ID3D11Buffer> buffer;
        const HRESULT hr = mState->mDevice->CreateBuffer(&bufferDesc, nullptr, &buffer);
        if (FAILED(hr) || !buffer) {
            return {};
        }

        return AdoptResource(new FRhiD3D11Buffer(desc, AltinaEngine::Move(buffer)));
#else
        return AdoptResource(new FRhiD3D11Buffer(desc));
#endif
    }

    auto FRhiD3D11Device::CreateTexture(const FRhiTextureDesc& desc) -> FRhiTextureRef {
#if AE_PLATFORM_WIN
        if (!mState || !mState->mDevice) {
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

        const D3D11_USAGE usage    = ToD3D11Usage(desc.mUsage);
        UINT              bindFlags = ToD3D11TextureBindFlags(desc.mBindFlags);
        const UINT        cpuAccess = ToD3D11CpuAccess(desc.mCpuAccess);

        if (usage == D3D11_USAGE_IMMUTABLE && cpuAccess != 0U) {
            return {};
        }
        if (usage == D3D11_USAGE_DYNAMIC
            && (cpuAccess & D3D11_CPU_ACCESS_WRITE) == 0U) {
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
            texDesc.Width     = desc.mWidth;
            texDesc.Height    = desc.mHeight;
            texDesc.Depth     = desc.mDepth;
            texDesc.MipLevels = desc.mMipLevels;
            texDesc.Format    = format;
            texDesc.Usage     = usage;
            texDesc.BindFlags = bindFlags;
            texDesc.CPUAccessFlags = cpuAccess;

            ComPtr<ID3D11Texture3D> texture;
            const HRESULT hr = mState->mDevice->CreateTexture3D(&texDesc, nullptr, &texture);
            if (FAILED(hr) || !texture) {
                return {};
            }

            ComPtr<ID3D11Resource> resource;
            texture.As(&resource);
            return AdoptResource(new FRhiD3D11Texture(desc, AltinaEngine::Move(resource)));
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
        texDesc.Width              = desc.mWidth;
        texDesc.Height             = desc.mHeight;
        texDesc.MipLevels          = desc.mMipLevels;
        texDesc.ArraySize          = desc.mArrayLayers;
        texDesc.Format             = format;
        texDesc.SampleDesc.Count   = desc.mSampleCount;
        texDesc.SampleDesc.Quality = 0U;
        texDesc.Usage              = usage;
        texDesc.BindFlags          = bindFlags;
        texDesc.CPUAccessFlags     = cpuAccess;

        ComPtr<ID3D11Texture2D> texture;
        const HRESULT hr = mState->mDevice->CreateTexture2D(&texDesc, nullptr, &texture);
        if (FAILED(hr) || !texture) {
            return {};
        }

        ComPtr<ID3D11Resource> resource;
        texture.As(&resource);
        return AdoptResource(new FRhiD3D11Texture(desc, AltinaEngine::Move(resource)));
#else
        return AdoptResource(new FRhiD3D11Texture(desc));
#endif
    }

    auto FRhiD3D11Device::CreateSampler(const FRhiSamplerDesc& desc) -> FRhiSamplerRef {
        return AdoptResource(new FRhiD3D11Sampler(desc));
    }

    auto FRhiD3D11Device::CreateShader(const FRhiShaderDesc& desc) -> FRhiShaderRef {
        return AdoptResource(new FRhiD3D11Shader(desc));
    }

    auto FRhiD3D11Device::CreateGraphicsPipeline(const FRhiGraphicsPipelineDesc& desc)
        -> FRhiPipelineRef {
        return AdoptResource(new FRhiD3D11GraphicsPipeline(desc));
    }

    auto FRhiD3D11Device::CreateComputePipeline(const FRhiComputePipelineDesc& desc)
        -> FRhiPipelineRef {
        return AdoptResource(new FRhiD3D11ComputePipeline(desc));
    }

    auto FRhiD3D11Device::CreatePipelineLayout(const FRhiPipelineLayoutDesc& desc)
        -> FRhiPipelineLayoutRef {
        return AdoptResource(new FRhiD3D11PipelineLayout(desc));
    }

    auto FRhiD3D11Device::CreateBindGroupLayout(const FRhiBindGroupLayoutDesc& desc)
        -> FRhiBindGroupLayoutRef {
        return AdoptResource(new FRhiD3D11BindGroupLayout(desc));
    }

    auto FRhiD3D11Device::CreateBindGroup(const FRhiBindGroupDesc& desc)
        -> FRhiBindGroupRef {
        return AdoptResource(new FRhiD3D11BindGroup(desc));
    }

    auto FRhiD3D11Device::CreateFence(bool /*signaled*/) -> FRhiFenceRef {
        return AdoptResource(new FRhiD3D11Fence());
    }

    auto FRhiD3D11Device::CreateSemaphore() -> FRhiSemaphoreRef {
        return AdoptResource(new FRhiD3D11Semaphore());
    }

    auto FRhiD3D11Device::CreateCommandPool(const FRhiCommandPoolDesc& desc)
        -> FRhiCommandPoolRef {
        return AdoptResource(new FRhiD3D11CommandPool(desc));
    }

} // namespace AltinaEngine::Rhi
