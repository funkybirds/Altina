#pragma once

#include "RhiD3D11API.h"
#include "Rhi/RhiDevice.h"

struct ID3D11Device;
struct ID3D11DeviceContext;

namespace AltinaEngine::Rhi {

    class AE_RHI_D3D11_API FRhiD3D11Device final : public FRhiDevice {
    public:
        FRhiD3D11Device(const FRhiDeviceDesc& desc, const FRhiAdapterDesc& adapterDesc,
            ID3D11Device* device, ID3D11DeviceContext* context, u32 featureLevel);
        ~FRhiD3D11Device() override;

        FRhiD3D11Device(const FRhiD3D11Device&) = delete;
        FRhiD3D11Device(FRhiD3D11Device&&) = delete;
        auto operator=(const FRhiD3D11Device&) -> FRhiD3D11Device& = delete;
        auto operator=(FRhiD3D11Device&&) -> FRhiD3D11Device& = delete;

        [[nodiscard]] auto GetNativeDevice() const noexcept -> ID3D11Device*;
        [[nodiscard]] auto GetImmediateContext() const noexcept -> ID3D11DeviceContext*;
        [[nodiscard]] auto GetFeatureLevel() const noexcept -> u32;

        auto CreateBuffer(const FRhiBufferDesc& desc) -> FRhiBufferRef override;
        auto CreateTexture(const FRhiTextureDesc& desc) -> FRhiTextureRef override;
        auto CreateSampler(const FRhiSamplerDesc& desc) -> FRhiSamplerRef override;
        auto CreateShader(const FRhiShaderDesc& desc) -> FRhiShaderRef override;

        auto CreateGraphicsPipeline(const FRhiGraphicsPipelineDesc& desc)
            -> FRhiPipelineRef override;
        auto CreateComputePipeline(const FRhiComputePipelineDesc& desc)
            -> FRhiPipelineRef override;
        auto CreatePipelineLayout(const FRhiPipelineLayoutDesc& desc)
            -> FRhiPipelineLayoutRef override;

        auto CreateBindGroupLayout(const FRhiBindGroupLayoutDesc& desc)
            -> FRhiBindGroupLayoutRef override;
        auto CreateBindGroup(const FRhiBindGroupDesc& desc) -> FRhiBindGroupRef override;

        auto CreateFence(u64 initialValue) -> FRhiFenceRef override;
        auto CreateSemaphore(bool timeline, u64 initialValue) -> FRhiSemaphoreRef override;

        auto CreateCommandPool(const FRhiCommandPoolDesc& desc)
            -> FRhiCommandPoolRef override;
        auto CreateCommandList(const FRhiCommandListDesc& desc)
            -> FRhiCommandListRef override;
        auto CreateCommandContext(const FRhiCommandContextDesc& desc)
            -> FRhiCommandContextRef override;

    private:
        struct FState;
        FState* mState = nullptr;
    };

} // namespace AltinaEngine::Rhi
