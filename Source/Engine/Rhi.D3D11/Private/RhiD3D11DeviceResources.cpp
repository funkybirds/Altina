#include "RhiD3D11/RhiD3D11Device.h"

#include "Rhi/RhiBuffer.h"
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
#if AE_PLATFORM_WIN
    using Microsoft::WRL::ComPtr;
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
#if AE_PLATFORM_WIN
            FRhiD3D11Sampler(const FRhiSamplerDesc& desc, ComPtr<ID3D11SamplerState> sampler)
                : FRhiSampler(desc), mSampler(AltinaEngine::Move(sampler)) {}
#else
            explicit FRhiD3D11Sampler(const FRhiSamplerDesc& desc) : FRhiSampler(desc) {}
#endif

        private:
#if AE_PLATFORM_WIN
            ComPtr<ID3D11SamplerState> mSampler;
#endif
        };
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

        return MakeResource<FRhiD3D11Buffer>(desc, AltinaEngine::Move(buffer));
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
            return MakeResource<FRhiD3D11Texture>(desc, AltinaEngine::Move(resource));
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
        return MakeResource<FRhiD3D11Texture>(desc, AltinaEngine::Move(resource));
#else
        return MakeResource<FRhiD3D11Texture>(desc);
#endif
    }

    auto FRhiD3D11Device::CreateSampler(const FRhiSamplerDesc& desc) -> FRhiSamplerRef {
#if AE_PLATFORM_WIN
        ID3D11Device* device = GetNativeDevice();
        if (device == nullptr) {
            return {};
        }

        D3D11_SAMPLER_DESC samplerDesc = {};
        samplerDesc.Filter             = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
        samplerDesc.AddressU           = D3D11_TEXTURE_ADDRESS_WRAP;
        samplerDesc.AddressV           = D3D11_TEXTURE_ADDRESS_WRAP;
        samplerDesc.AddressW           = D3D11_TEXTURE_ADDRESS_WRAP;
        samplerDesc.MipLODBias         = 0.0f;
        samplerDesc.MaxAnisotropy      = 1;
        samplerDesc.ComparisonFunc     = D3D11_COMPARISON_ALWAYS;
        samplerDesc.BorderColor[0]     = 0.0f;
        samplerDesc.BorderColor[1]     = 0.0f;
        samplerDesc.BorderColor[2]     = 0.0f;
        samplerDesc.BorderColor[3]     = 0.0f;
        samplerDesc.MinLOD             = 0.0f;
        samplerDesc.MaxLOD             = D3D11_FLOAT32_MAX;

        ComPtr<ID3D11SamplerState> sampler;
        const HRESULT              hr = device->CreateSamplerState(&samplerDesc, &sampler);
        if (FAILED(hr) || (sampler == nullptr)) {
            return {};
        }

        return MakeResource<FRhiD3D11Sampler>(desc, AltinaEngine::Move(sampler));
#else
        return MakeResource<FRhiD3D11Sampler>(desc);
#endif
    }
} // namespace AltinaEngine::Rhi
