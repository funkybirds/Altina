#pragma once

#include "RhiGeneralAPI.h"
#include "Rhi/RhiResource.h"
#include "Rhi/RhiStructs.h"

namespace AltinaEngine::Rhi {
    namespace Container = Core::Container;
    using Container::FStringView;

    class AE_RHI_GENERAL_API FRhiTexture : public FRhiResource {
    public:
        explicit FRhiTexture(
            const FRhiTextureDesc& desc, FRhiResourceDeleteQueue* deleteQueue = nullptr) noexcept;

        ~FRhiTexture() override;

        FRhiTexture(const FRhiTexture&)                                  = delete;
        FRhiTexture(FRhiTexture&&)                                       = delete;
        auto               operator=(const FRhiTexture&) -> FRhiTexture& = delete;
        auto               operator=(FRhiTexture&&) -> FRhiTexture&      = delete;

        [[nodiscard]] auto GetDesc() const noexcept -> const FRhiTextureDesc& { return mDesc; }
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
        FRhiTextureDesc mDesc;
    };

} // namespace AltinaEngine::Rhi
