#pragma once

#include "RhiGeneralAPI.h"
#include "Rhi/RhiResource.h"
#include "Rhi/RhiStructs.h"

namespace AltinaEngine::Rhi {
    using Core::Container::FStringView;

    class AE_RHI_GENERAL_API FRhiPipeline : public FRhiResource {
    public:
        explicit FRhiPipeline(const FRhiGraphicsPipelineDesc& desc,
            FRhiResourceDeleteQueue* deleteQueue = nullptr) noexcept;

        explicit FRhiPipeline(const FRhiComputePipelineDesc& desc,
            FRhiResourceDeleteQueue* deleteQueue = nullptr) noexcept;

        ~FRhiPipeline() override;

        FRhiPipeline(const FRhiPipeline&) = delete;
        FRhiPipeline(FRhiPipeline&&) = delete;
        auto operator=(const FRhiPipeline&) -> FRhiPipeline& = delete;
        auto operator=(FRhiPipeline&&) -> FRhiPipeline& = delete;

        [[nodiscard]] auto IsGraphics() const noexcept -> bool { return mIsGraphics; }
        [[nodiscard]] auto GetGraphicsDesc() const noexcept -> const FRhiGraphicsPipelineDesc& {
            return mGraphicsDesc;
        }
        [[nodiscard]] auto GetComputeDesc() const noexcept -> const FRhiComputePipelineDesc& {
            return mComputeDesc;
        }

        [[nodiscard]] auto GetDebugName() const noexcept -> FStringView {
            return mIsGraphics ? mGraphicsDesc.mDebugName.ToView()
                               : mComputeDesc.mDebugName.ToView();
        }

    private:
        FRhiGraphicsPipelineDesc mGraphicsDesc;
        FRhiComputePipelineDesc  mComputeDesc;
        bool                     mIsGraphics = true;
    };

} // namespace AltinaEngine::Rhi
