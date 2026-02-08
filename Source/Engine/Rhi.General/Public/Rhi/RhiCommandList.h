#pragma once

#include "RhiGeneralAPI.h"
#include "Rhi/RhiResource.h"
#include "Rhi/RhiStructs.h"
#include "Container/StringView.h"

namespace AltinaEngine::Rhi {
    using Core::Container::FStringView;

    class AE_RHI_GENERAL_API FRhiCommandList : public FRhiResource {
    public:
        explicit FRhiCommandList(const FRhiCommandListDesc& desc,
            FRhiResourceDeleteQueue* deleteQueue = nullptr) noexcept;

        ~FRhiCommandList() override;

        FRhiCommandList(const FRhiCommandList&) = delete;
        FRhiCommandList(FRhiCommandList&&) = delete;
        auto operator=(const FRhiCommandList&) -> FRhiCommandList& = delete;
        auto operator=(FRhiCommandList&&) -> FRhiCommandList& = delete;

        [[nodiscard]] auto GetDesc() const noexcept -> const FRhiCommandListDesc& { return mDesc; }
        [[nodiscard]] auto GetQueueType() const noexcept -> ERhiQueueType {
            return mDesc.mQueueType;
        }
        [[nodiscard]] auto GetListType() const noexcept -> ERhiCommandListType {
            return mDesc.mListType;
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

        virtual void Reset(FRhiCommandPool* pool) = 0;
        virtual void Close() = 0;

    private:
        FRhiCommandListDesc mDesc;
    };

} // namespace AltinaEngine::Rhi
