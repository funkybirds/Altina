#pragma once

#include "RhiGeneralAPI.h"
#include "Rhi/RhiResource.h"
#include "Rhi/RhiStructs.h"

namespace AltinaEngine::Rhi {
    using Core::Container::FStringView;

    class AE_RHI_GENERAL_API FRhiSemaphore : public FRhiResource {
    public:
        explicit FRhiSemaphore(FRhiResourceDeleteQueue* deleteQueue = nullptr) noexcept;

        ~FRhiSemaphore() override;

        [[nodiscard]] auto GetDebugName() const noexcept -> FStringView {
            return mDebugName.ToView();
        }
        void SetDebugName(FStringView name) {
            mDebugName.Clear();
            if (!name.IsEmpty()) {
                mDebugName.Append(name.Data(), name.Length());
            }
        }

    protected:
        Core::Container::FString mDebugName;
    };

} // namespace AltinaEngine::Rhi
