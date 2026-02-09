#pragma once

#include "RhiGeneralAPI.h"
#include "Rhi/RhiResource.h"
#include "Rhi/RhiStructs.h"

namespace AltinaEngine::Rhi {
    using Core::Container::FStringView;

    class AE_RHI_GENERAL_API FRhiBuffer : public FRhiResource {
    public:
        struct FLockResult {
            void*              mData   = nullptr;
            u64                mOffset = 0ULL;
            u64                mSize   = 0ULL;
            ERhiBufferLockMode mMode   = ERhiBufferLockMode::Read;
            void*              mHandle = nullptr;

            [[nodiscard]] auto IsValid() const noexcept -> bool { return mData != nullptr; }
        };

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

        virtual auto Lock(u64 offset, u64 size, ERhiBufferLockMode mode) -> FLockResult;
        virtual void Unlock(FLockResult& lock);

    private:
        FRhiBufferDesc mDesc;
    };

} // namespace AltinaEngine::Rhi
