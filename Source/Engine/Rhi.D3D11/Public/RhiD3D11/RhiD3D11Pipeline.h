#pragma once

#include "RhiD3D11API.h"
#include "Rhi/RhiPipeline.h"

struct ID3D11Device;
struct ID3D11InputLayout;

namespace AltinaEngine::Rhi {
    using Core::Container::TVector;

    struct FD3D11BindingMappingEntry {
        EShaderStage     mStage   = EShaderStage::Vertex;
        ERhiBindingType  mType    = ERhiBindingType::ConstantBuffer;
        u32              mSet     = 0U;
        u32              mBinding = 0U;
        u32              mRegister = 0U;
        u32              mSpace    = 0U;
    };

    class AE_RHI_D3D11_API FRhiD3D11GraphicsPipeline final : public FRhiPipeline {
    public:
        FRhiD3D11GraphicsPipeline(const FRhiGraphicsPipelineDesc& desc, ID3D11Device* device);
        ~FRhiD3D11GraphicsPipeline() override;

        [[nodiscard]] auto GetInputLayout() const noexcept -> ID3D11InputLayout*;
        [[nodiscard]] auto GetBindingMappings() const noexcept
            -> const TVector<FD3D11BindingMappingEntry>&;

    private:
        struct FState;
        FState* mState = nullptr;
        TVector<FD3D11BindingMappingEntry> mBindings;
    };

    class AE_RHI_D3D11_API FRhiD3D11ComputePipeline final : public FRhiPipeline {
    public:
        explicit FRhiD3D11ComputePipeline(const FRhiComputePipelineDesc& desc);
        ~FRhiD3D11ComputePipeline() override;

        [[nodiscard]] auto GetBindingMappings() const noexcept
            -> const TVector<FD3D11BindingMappingEntry>&;

    private:
        TVector<FD3D11BindingMappingEntry> mBindings;
    };

} // namespace AltinaEngine::Rhi
