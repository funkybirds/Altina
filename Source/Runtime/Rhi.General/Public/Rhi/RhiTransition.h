#pragma once

#include "RhiGeneralAPI.h"
#include "Rhi/RhiEnums.h"
#include "Rhi/RhiResource.h"
#include "Rhi/RhiSemaphore.h"
#include "Container/String.h"
#include "Container/StringView.h"
#include "Types/Aliases.h"

namespace AltinaEngine::Rhi {
    namespace Container = Core::Container;
    using Container::FString;
    using Container::FStringView;

    struct FRhiTransitionDesc {
        ERhiQueueType mSrcQueue = ERhiQueueType::Graphics;
        ERhiQueueType mDstQueue = ERhiQueueType::Graphics;
    };

    class AE_RHI_GENERAL_API FRhiTransition : public FRhiResource {
    public:
        explicit FRhiTransition(const FRhiTransitionDesc& desc,
            FRhiResourceDeleteQueue*                      deleteQueue = nullptr) noexcept;
        ~FRhiTransition() override;

        FRhiTransition(const FRhiTransition&)                                  = delete;
        FRhiTransition(FRhiTransition&&)                                       = delete;
        auto               operator=(const FRhiTransition&) -> FRhiTransition& = delete;
        auto               operator=(FRhiTransition&&) -> FRhiTransition&      = delete;

        [[nodiscard]] auto GetDesc() const noexcept -> const FRhiTransitionDesc& { return mDesc; }

        [[nodiscard]] auto GetDebugName() const noexcept -> FStringView {
            return mDebugName.ToView();
        }
        void SetDebugName(FStringView name) {
            mDebugName.Clear();
            if (!name.IsEmpty()) {
                mDebugName.Append(name.Data(), name.Length());
            }
        }

        [[nodiscard]] virtual auto GetSemaphore() const noexcept -> FRhiSemaphore* = 0;
        [[nodiscard]] virtual auto GetSignalValue() const noexcept -> u64          = 0;
        virtual void               SetSignalValue(u64 value)                       = 0;

    private:
        FRhiTransitionDesc mDesc;
        FString            mDebugName;
    };

} // namespace AltinaEngine::Rhi
