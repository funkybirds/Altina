#pragma once

#include "RhiD3D11API.h"
#include "Rhi/RhiBuffer.h"
#include "Rhi/RhiSampler.h"
#include "Rhi/RhiTexture.h"
#include "Container/SmartPtr.h"

using AltinaEngine::Core::Container::TOwner;

struct ID3D11Buffer;
struct ID3D11Resource;
struct ID3D11SamplerState;
struct ID3D11RenderTargetView;
struct ID3D11DepthStencilView;
struct ID3D11ShaderResourceView;
struct ID3D11UnorderedAccessView;

namespace AltinaEngine::Rhi {
    namespace Container = Core::Container;
    class AE_RHI_D3D11_API FRhiD3D11Buffer final : public FRhiBuffer {
    public:
        FRhiD3D11Buffer(const FRhiBufferDesc& desc, ID3D11Buffer* buffer,
            ID3D11ShaderResourceView*  shaderResourceView,
            ID3D11UnorderedAccessView* unorderedAccessView);
        explicit FRhiD3D11Buffer(const FRhiBufferDesc& desc);
        ~FRhiD3D11Buffer() override;

        FRhiD3D11Buffer(const FRhiD3D11Buffer&)                    = delete;
        FRhiD3D11Buffer(FRhiD3D11Buffer&&)                         = delete;
        auto operator=(const FRhiD3D11Buffer&) -> FRhiD3D11Buffer& = delete;
        auto operator=(FRhiD3D11Buffer&&) -> FRhiD3D11Buffer&      = delete;

        auto Lock(u64 offset, u64 size, ERhiBufferLockMode mode) -> FLockResult override;
        void Unlock(FLockResult& lock) override;

        [[nodiscard]] auto GetNativeBuffer() const noexcept -> ID3D11Buffer*;
        [[nodiscard]] auto GetShaderResourceView() const noexcept -> ID3D11ShaderResourceView*;
        [[nodiscard]] auto GetUnorderedAccessView() const noexcept -> ID3D11UnorderedAccessView*;

    private:
        struct FState;
        TOwner<FState> mState;
    };

    class AE_RHI_D3D11_API FRhiD3D11Texture final : public FRhiTexture {
    public:
        FRhiD3D11Texture(const FRhiTextureDesc& desc, ID3D11Resource* resource,
            ID3D11RenderTargetView* renderTargetView, ID3D11DepthStencilView* depthStencilView,
            ID3D11ShaderResourceView*  shaderResourceView,
            ID3D11UnorderedAccessView* unorderedAccessView);
        explicit FRhiD3D11Texture(const FRhiTextureDesc& desc);
        ~FRhiD3D11Texture() override;

        FRhiD3D11Texture(const FRhiD3D11Texture&)                                  = delete;
        FRhiD3D11Texture(FRhiD3D11Texture&&)                                       = delete;
        auto               operator=(const FRhiD3D11Texture&) -> FRhiD3D11Texture& = delete;
        auto               operator=(FRhiD3D11Texture&&) -> FRhiD3D11Texture&      = delete;

        [[nodiscard]] auto GetNativeResource() const noexcept -> ID3D11Resource*;
        [[nodiscard]] auto GetRenderTargetView() const noexcept -> ID3D11RenderTargetView*;
        [[nodiscard]] auto GetDepthStencilView() const noexcept -> ID3D11DepthStencilView*;
        [[nodiscard]] auto GetShaderResourceView() const noexcept -> ID3D11ShaderResourceView*;
        [[nodiscard]] auto GetUnorderedAccessView() const noexcept -> ID3D11UnorderedAccessView*;

    private:
        struct FState;
        TOwner<FState> mState;
    };

    class AE_RHI_D3D11_API FRhiD3D11Sampler final : public FRhiSampler {
    public:
        FRhiD3D11Sampler(const FRhiSamplerDesc& desc, ID3D11SamplerState* sampler);
        explicit FRhiD3D11Sampler(const FRhiSamplerDesc& desc);
        ~FRhiD3D11Sampler() override;

        FRhiD3D11Sampler(const FRhiD3D11Sampler&)                                  = delete;
        FRhiD3D11Sampler(FRhiD3D11Sampler&&)                                       = delete;
        auto               operator=(const FRhiD3D11Sampler&) -> FRhiD3D11Sampler& = delete;
        auto               operator=(FRhiD3D11Sampler&&) -> FRhiD3D11Sampler&      = delete;

        [[nodiscard]] auto GetNativeSampler() const noexcept -> ID3D11SamplerState*;

    private:
        struct FState;
        TOwner<FState> mState;
    };
} // namespace AltinaEngine::Rhi
