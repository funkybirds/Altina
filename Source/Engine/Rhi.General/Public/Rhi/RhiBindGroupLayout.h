#pragma once

#include "RhiGeneralAPI.h"
#include "Rhi/RhiResource.h"
#include "Rhi/RhiStructs.h"

namespace AltinaEngine::Rhi {
    using Core::Container::FStringView;

    class AE_RHI_GENERAL_API FRhiBindGroupLayout : public FRhiResource {
    public:
        explicit FRhiBindGroupLayout(const FRhiBindGroupLayoutDesc& desc,
            FRhiResourceDeleteQueue* deleteQueue = nullptr) noexcept;

        ~FRhiBindGroupLayout() override;

        [[nodiscard]] auto GetDesc() const noexcept -> const FRhiBindGroupLayoutDesc& {
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

    protected:
        FRhiBindGroupLayoutDesc mDesc;
    };

} // namespace AltinaEngine::Rhi
