#pragma once

#include "RhiGeneralAPI.h"
#include "Rhi/RhiResource.h"
#include "Rhi/RhiStructs.h"

namespace AltinaEngine::Rhi {
    using Core::Container::FStringView;

    class AE_RHI_GENERAL_API FRhiBuffer : public FRhiResource {
    public:
        explicit FRhiBuffer(const FRhiBufferDesc& desc,
            FRhiResourceDeleteQueue* deleteQueue = nullptr) noexcept;

        ~FRhiBuffer() override;

        FRhiBuffer(const FRhiBuffer&) = delete;
        FRhiBuffer(FRhiBuffer&&) = delete;
        auto operator=(const FRhiBuffer&) -> FRhiBuffer& = delete;
        auto operator=(FRhiBuffer&&) -> FRhiBuffer& = delete;

        [[nodiscard]] auto GetDesc() const noexcept -> const FRhiBufferDesc& { return mDesc; }
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
        FRhiBufferDesc mDesc;
    };

} // namespace AltinaEngine::Rhi
