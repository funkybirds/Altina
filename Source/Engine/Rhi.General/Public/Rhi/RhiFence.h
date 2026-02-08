#pragma once

#include "RhiGeneralAPI.h"
#include "Rhi/RhiResource.h"
#include "Rhi/RhiStructs.h"

namespace AltinaEngine::Rhi {
    using Core::Container::FStringView;

    class AE_RHI_GENERAL_API FRhiFence : public FRhiResource {
    public:
        explicit FRhiFence(FRhiResourceDeleteQueue* deleteQueue = nullptr) noexcept;

        ~FRhiFence() override;

        [[nodiscard]] auto GetDebugName() const noexcept -> FStringView {
            return mDebugName.ToView();
        }
        void SetDebugName(FStringView name) {
            mDebugName.Clear();
            if (!name.IsEmpty()) {
                mDebugName.Append(name.Data(), name.Length());
            }
        }

        [[nodiscard]] virtual auto GetCompletedValue() const noexcept -> u64 = 0;
        virtual void SignalCPU(u64 value) = 0;
        virtual void WaitCPU(u64 value) = 0;
        virtual void Reset(u64 value) = 0;

    protected:
        Core::Container::FString mDebugName;
    };

} // namespace AltinaEngine::Rhi
