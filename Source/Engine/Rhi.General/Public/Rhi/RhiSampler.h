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

    protected:
        FRhiSamplerDesc mDesc;
    };

} // namespace AltinaEngine::Rhi
