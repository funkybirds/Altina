#pragma once

#include "RhiGeneralAPI.h"
#include "Rhi/RhiResource.h"
#include "Rhi/RhiStructs.h"

namespace AltinaEngine::Rhi {
    namespace Container = Core::Container;
    using Container::FStringView;

    class AE_RHI_GENERAL_API FRhiSemaphore : public FRhiResource {
    public:
        explicit FRhiSemaphore(FRhiResourceDeleteQueue* deleteQueue = nullptr) noexcept;

        ~FRhiSemaphore() override;

        FRhiSemaphore(const FRhiSemaphore&)                                  = delete;
        FRhiSemaphore(FRhiSemaphore&&)                                       = delete;
        auto               operator=(const FRhiSemaphore&) -> FRhiSemaphore& = delete;
        auto               operator=(FRhiSemaphore&&) -> FRhiSemaphore&      = delete;

        [[nodiscard]] auto GetDebugName() const noexcept -> FStringView {
            return mDebugName.ToView();
        }
        void SetDebugName(FStringView name) {
            mDebugName.Clear();
            if (!name.IsEmpty()) {
                mDebugName.Append(name.Data(), name.Length());
            }
        }

        [[nodiscard]] virtual auto IsTimeline() const noexcept -> bool     = 0;
        [[nodiscard]] virtual auto GetCurrentValue() const noexcept -> u64 = 0;

    private:
        FString mDebugName;
    };

} // namespace AltinaEngine::Rhi
