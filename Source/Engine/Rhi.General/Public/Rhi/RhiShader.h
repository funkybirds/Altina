#pragma once

#include "RhiGeneralAPI.h"
#include "Rhi/RhiResource.h"
#include "Rhi/RhiStructs.h"

namespace AltinaEngine::Rhi {
    namespace Container = Core::Container;
    using Container::FStringView;

    class AE_RHI_GENERAL_API FRhiShader : public FRhiResource {
    public:
        explicit FRhiShader(
            const FRhiShaderDesc& desc, FRhiResourceDeleteQueue* deleteQueue = nullptr) noexcept;

        ~FRhiShader() override;

        FRhiShader(const FRhiShader&)                                  = delete;
        FRhiShader(FRhiShader&&)                                       = delete;
        auto               operator=(const FRhiShader&) -> FRhiShader& = delete;
        auto               operator=(FRhiShader&&) -> FRhiShader&      = delete;

        [[nodiscard]] auto GetDesc() const noexcept -> const FRhiShaderDesc& { return mDesc; }
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
        FRhiShaderDesc mDesc;
    };

} // namespace AltinaEngine::Rhi
