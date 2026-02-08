#pragma once

#include "RhiGeneralAPI.h"
#include "Rhi/RhiResource.h"
#include "Rhi/RhiStructs.h"

namespace AltinaEngine::Rhi {
    using Core::Container::FStringView;

    class AE_RHI_GENERAL_API FRhiSampler : public FRhiResource {
    public:
        explicit FRhiSampler(const FRhiSamplerDesc& desc,
            FRhiResourceDeleteQueue* deleteQueue = nullptr) noexcept;

        ~FRhiSampler() override;

        FRhiSampler(const FRhiSampler&) = delete;
        FRhiSampler(FRhiSampler&&) = delete;
        auto operator=(const FRhiSampler&) -> FRhiSampler& = delete;
        auto operator=(FRhiSampler&&) -> FRhiSampler& = delete;

        [[nodiscard]] auto GetDesc() const noexcept -> const FRhiSamplerDesc& { return mDesc; }
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
        FRhiSamplerDesc mDesc;
    };

} // namespace AltinaEngine::Rhi
