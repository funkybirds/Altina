#pragma once

#include "RhiGeneralAPI.h"
#include "Rhi/RhiResource.h"
#include "Rhi/RhiStructs.h"
#include "Container/StringView.h"

namespace AltinaEngine::Rhi {
    namespace Container = Core::Container;
    using Container::FStringView;

    class AE_RHI_GENERAL_API FRhiViewport : public FRhiResource {
    public:
        explicit FRhiViewport(
            const FRhiViewportDesc& desc, FRhiResourceDeleteQueue* deleteQueue = nullptr) noexcept;

        ~FRhiViewport() override;

        FRhiViewport(const FRhiViewport&)                                          = delete;
        FRhiViewport(FRhiViewport&&)                                               = delete;
        auto                       operator=(const FRhiViewport&) -> FRhiViewport& = delete;
        auto                       operator=(FRhiViewport&&) -> FRhiViewport&      = delete;

        virtual void               Resize(u32 width, u32 height)                  = 0;
        [[nodiscard]] virtual auto GetBackBuffer() const noexcept -> FRhiTexture* = 0;
        virtual void               Present(const FRhiPresentInfo& info)           = 0;

        [[nodiscard]] auto GetDesc() const noexcept -> const FRhiViewportDesc& { return mDesc; }
        [[nodiscard]] auto GetDebugName() const noexcept -> FStringView {
            return mDesc.mDebugName.ToView();
        }
        void SetDebugName(FStringView name) {
            mDesc.mDebugName.Clear();
            if (!name.IsEmpty()) {
                mDesc.mDebugName.Append(name.Data(), name.Length());
            }
        }

    protected:
        void UpdateExtent(u32 width, u32 height) noexcept {
            mDesc.mWidth  = width;
            mDesc.mHeight = height;
        }

    private:
        FRhiViewportDesc mDesc;
    };

} // namespace AltinaEngine::Rhi
