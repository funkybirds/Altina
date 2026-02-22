#pragma once

#include "RhiGeneralAPI.h"
#include "Rhi/RhiBuffer.h"
#include "Rhi/RhiRefs.h"
#include "Rhi/RhiResource.h"
#include "Rhi/RhiStructs.h"
#include "Rhi/RhiTexture.h"

namespace AltinaEngine::Rhi {
    namespace Container = Core::Container;
    using Container::FStringView;

    class AE_RHI_GENERAL_API FRhiResourceView : public FRhiResource {
    public:
        explicit FRhiResourceView(
            ERhiResourceViewType type, FRhiResourceDeleteQueue* deleteQueue = nullptr) noexcept;

        ~FRhiResourceView() override;

        FRhiResourceView(const FRhiResourceView&)                                  = delete;
        FRhiResourceView(FRhiResourceView&&)                                       = delete;
        auto               operator=(const FRhiResourceView&) -> FRhiResourceView& = delete;
        auto               operator=(FRhiResourceView&&) -> FRhiResourceView&      = delete;

        [[nodiscard]] auto GetViewType() const noexcept -> ERhiResourceViewType {
            return mViewType;
        }

    private:
        ERhiResourceViewType mViewType = ERhiResourceViewType::Unknown;
    };

    class AE_RHI_GENERAL_API FRhiShaderResourceView : public FRhiResourceView {
    public:
        explicit FRhiShaderResourceView(const FRhiShaderResourceViewDesc& desc,
            FRhiResourceDeleteQueue* deleteQueue = nullptr) noexcept;

        ~FRhiShaderResourceView() override;

        FRhiShaderResourceView(const FRhiShaderResourceView&)                    = delete;
        FRhiShaderResourceView(FRhiShaderResourceView&&)                         = delete;
        auto operator=(const FRhiShaderResourceView&) -> FRhiShaderResourceView& = delete;
        auto operator=(FRhiShaderResourceView&&) -> FRhiShaderResourceView&      = delete;

        [[nodiscard]] auto GetDesc() const noexcept -> const FRhiShaderResourceViewDesc& {
            return mDesc;
        }
        [[nodiscard]] auto GetTexture() const noexcept -> FRhiTexture* { return mTexture.Get(); }
        [[nodiscard]] auto GetBuffer() const noexcept -> FRhiBuffer* { return mBuffer.Get(); }
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
        FRhiShaderResourceViewDesc mDesc;
        FRhiTextureRef             mTexture;
        FRhiBufferRef              mBuffer;
    };

    class AE_RHI_GENERAL_API FRhiUnorderedAccessView : public FRhiResourceView {
    public:
        explicit FRhiUnorderedAccessView(const FRhiUnorderedAccessViewDesc& desc,
            FRhiResourceDeleteQueue* deleteQueue = nullptr) noexcept;

        ~FRhiUnorderedAccessView() override;

        FRhiUnorderedAccessView(const FRhiUnorderedAccessView&)                    = delete;
        FRhiUnorderedAccessView(FRhiUnorderedAccessView&&)                         = delete;
        auto operator=(const FRhiUnorderedAccessView&) -> FRhiUnorderedAccessView& = delete;
        auto operator=(FRhiUnorderedAccessView&&) -> FRhiUnorderedAccessView&      = delete;

        [[nodiscard]] auto GetDesc() const noexcept -> const FRhiUnorderedAccessViewDesc& {
            return mDesc;
        }
        [[nodiscard]] auto GetTexture() const noexcept -> FRhiTexture* { return mTexture.Get(); }
        [[nodiscard]] auto GetBuffer() const noexcept -> FRhiBuffer* { return mBuffer.Get(); }
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
        FRhiUnorderedAccessViewDesc mDesc;
        FRhiTextureRef              mTexture;
        FRhiBufferRef               mBuffer;
    };

    class AE_RHI_GENERAL_API FRhiRenderTargetView : public FRhiResourceView {
    public:
        explicit FRhiRenderTargetView(const FRhiRenderTargetViewDesc& desc,
            FRhiResourceDeleteQueue* deleteQueue = nullptr) noexcept;

        ~FRhiRenderTargetView() override;

        FRhiRenderTargetView(const FRhiRenderTargetView&)                                  = delete;
        FRhiRenderTargetView(FRhiRenderTargetView&&)                                       = delete;
        auto               operator=(const FRhiRenderTargetView&) -> FRhiRenderTargetView& = delete;
        auto               operator=(FRhiRenderTargetView&&) -> FRhiRenderTargetView&      = delete;

        [[nodiscard]] auto GetDesc() const noexcept -> const FRhiRenderTargetViewDesc& {
            return mDesc;
        }
        [[nodiscard]] auto GetTexture() const noexcept -> FRhiTexture* { return mTexture.Get(); }
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
        FRhiRenderTargetViewDesc mDesc;
        FRhiTextureRef           mTexture;
    };

    class AE_RHI_GENERAL_API FRhiDepthStencilView : public FRhiResourceView {
    public:
        explicit FRhiDepthStencilView(const FRhiDepthStencilViewDesc& desc,
            FRhiResourceDeleteQueue* deleteQueue = nullptr) noexcept;

        ~FRhiDepthStencilView() override;

        FRhiDepthStencilView(const FRhiDepthStencilView&)                                  = delete;
        FRhiDepthStencilView(FRhiDepthStencilView&&)                                       = delete;
        auto               operator=(const FRhiDepthStencilView&) -> FRhiDepthStencilView& = delete;
        auto               operator=(FRhiDepthStencilView&&) -> FRhiDepthStencilView&      = delete;

        [[nodiscard]] auto GetDesc() const noexcept -> const FRhiDepthStencilViewDesc& {
            return mDesc;
        }
        [[nodiscard]] auto GetTexture() const noexcept -> FRhiTexture* { return mTexture.Get(); }
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
        FRhiDepthStencilViewDesc mDesc;
        FRhiTextureRef           mTexture;
    };

} // namespace AltinaEngine::Rhi
