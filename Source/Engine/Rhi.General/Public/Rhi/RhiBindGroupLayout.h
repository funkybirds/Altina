#pragma once

#include "RhiGeneralAPI.h"
#include "Rhi/RhiResource.h"
#include "Rhi/RhiStructs.h"

namespace AltinaEngine::Rhi {
    namespace Container = Core::Container;
    using Container::FStringView;

    class AE_RHI_GENERAL_API FRhiBindGroupLayout : public FRhiResource {
    public:
        explicit FRhiBindGroupLayout(const FRhiBindGroupLayoutDesc& desc,
            FRhiResourceDeleteQueue*                                deleteQueue = nullptr) noexcept;

        ~FRhiBindGroupLayout() override;

        FRhiBindGroupLayout(const FRhiBindGroupLayout&)                                  = delete;
        FRhiBindGroupLayout(FRhiBindGroupLayout&&)                                       = delete;
        auto               operator=(const FRhiBindGroupLayout&) -> FRhiBindGroupLayout& = delete;
        auto               operator=(FRhiBindGroupLayout&&) -> FRhiBindGroupLayout&      = delete;

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

    private:
        FRhiBindGroupLayoutDesc mDesc;
    };

} // namespace AltinaEngine::Rhi
