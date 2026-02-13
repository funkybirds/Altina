#pragma once

#include "RhiGeneralAPI.h"
#include "Rhi/RhiResource.h"
#include "Rhi/RhiStructs.h"

namespace AltinaEngine::Rhi {
    namespace Container = Core::Container;
    using Container::FStringView;

    class AE_RHI_GENERAL_API FRhiCommandPool : public FRhiResource {
    public:
        explicit FRhiCommandPool(const FRhiCommandPoolDesc& desc,
            FRhiResourceDeleteQueue*                        deleteQueue = nullptr) noexcept;

        ~FRhiCommandPool() override;

        [[nodiscard]] auto GetDesc() const noexcept -> const FRhiCommandPoolDesc& { return mDesc; }
        [[nodiscard]] auto GetDebugName() const noexcept -> FStringView {
            return mDesc.mDebugName.ToView();
        }
        void SetDebugName(FStringView name) {
            mDesc.mDebugName.Clear();
            if (!name.IsEmpty()) {
                mDesc.mDebugName.Append(name.Data(), name.Length());
            }
        }

        virtual void Reset() = 0;

    private:
        FRhiCommandPoolDesc mDesc;
    };

} // namespace AltinaEngine::Rhi
