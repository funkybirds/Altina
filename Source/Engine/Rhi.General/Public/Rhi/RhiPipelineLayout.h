#pragma once

#include "RhiGeneralAPI.h"
#include "Rhi/RhiResource.h"
#include "Rhi/RhiStructs.h"

namespace AltinaEngine::Rhi {
    using Core::Container::FStringView;

    class AE_RHI_GENERAL_API FRhiPipelineLayout : public FRhiResource {
    public:
        explicit FRhiPipelineLayout(const FRhiPipelineLayoutDesc& desc,
            FRhiResourceDeleteQueue* deleteQueue = nullptr) noexcept;

        ~FRhiPipelineLayout() override;

        FRhiPipelineLayout(const FRhiPipelineLayout&) = delete;
        FRhiPipelineLayout(FRhiPipelineLayout&&) = delete;
        auto operator=(const FRhiPipelineLayout&) -> FRhiPipelineLayout& = delete;
        auto operator=(FRhiPipelineLayout&&) -> FRhiPipelineLayout& = delete;

        [[nodiscard]] auto GetDesc() const noexcept -> const FRhiPipelineLayoutDesc& {
            return mDesc;
        }
        [[nodiscard]] auto GetDebugName() const noexcept -> FStringView {
            return mDesc.mDebugName.ToView();
        }
        void SetDebugName(FStringView name) {
            mDesc.mDebugName.Clear();
            if (!name.IsEmpty()) {
                mDesc.mDebugName.Append(name.Data(), name.Length());
            }
        }

    private:
        FRhiPipelineLayoutDesc mDesc;
    };

} // namespace AltinaEngine::Rhi
