#pragma once

#include "RenderCoreAPI.h"

#include "Jobs/JobSystem.h"
#include "Threading/Atomic.h"
#include "Types/NonCopyable.h"

#include "Container/Vector.h"
#include "Rhi/RhiBuffer.h"
#include "Rhi/RhiEnums.h"
#include "Rhi/RhiResourceView.h"
#include "Rhi/RhiRefs.h"
#include "Rhi/RhiStructs.h"

using AltinaEngine::Core::Container::TVector;
namespace AltinaEngine::RenderCore {
    class AE_RENDER_CORE_API FRenderResource : public FNonCopyableClass {
    public:
        FRenderResource() = default;
        // Caller must ensure ReleaseResource() is invoked before destruction.
        ~FRenderResource() override;

        FRenderResource(FRenderResource&&) noexcept            = default;
        auto operator=(FRenderResource&&) noexcept -> FRenderResource& = default;

        void               InitResource() noexcept;
        void               ReleaseResource() noexcept;
        void               UpdateResource() noexcept;

        void               WaitForInit() noexcept;
        void               WaitForRelease() noexcept;

        [[nodiscard]] auto IsInitialized() const noexcept -> bool;

    protected:
        virtual void InitRHI()    = 0;
        virtual void ReleaseRHI() = 0;
        virtual void UpdateRHI() {}
        virtual void OnInitComplete() {}

    private:
        enum class EState : i32 {
            Uninitialized = 0,
            InitPending,
            Initialized,
            ReleasePending
        };

        Core::Threading::TAtomic<i32> mState{ static_cast<i32>(EState::Uninitialized) };
        Core::Jobs::FJobHandle        mInitHandle{};
        Core::Jobs::FJobHandle        mReleaseHandle{};
    };

    class AE_RENDER_CORE_API FPositionBuffer : public FRenderResource {
    public:
        FPositionBuffer() noexcept;
        explicit FPositionBuffer(const Rhi::FRhiBufferDesc& desc) noexcept;

        void               SetData(const void* data, u32 sizeBytes, u32 strideBytes);

        [[nodiscard]] auto GetBuffer() const noexcept -> Rhi::FRhiBufferRef { return mBuffer; }
        [[nodiscard]] auto GetView() const noexcept -> Rhi::FRhiVertexBufferView;

        [[nodiscard]] auto GetSizeBytes() const noexcept -> u64 { return mDesc.mSizeBytes; }
        [[nodiscard]] auto GetStrideBytes() const noexcept -> u32 { return mStrideBytes; }

        [[nodiscard]] auto GetElementCount() const noexcept -> u32 {
            if (mStrideBytes == 0U) {
                return 0U;
            }
            return static_cast<u32>(mDesc.mSizeBytes / static_cast<u64>(mStrideBytes));
        }

    protected:
        void InitRHI() override;
        void ReleaseRHI() override;
        void UpdateRHI() override;

    private:
        Rhi::FRhiBufferDesc mDesc{};
        u32                 mStrideBytes = 0U;
        TVector<u8>         mStagingData;
        Rhi::FRhiBufferRef  mBuffer;
    };

    class AE_RENDER_CORE_API FVertexTangentBuffer : public FRenderResource {
    public:
        FVertexTangentBuffer() noexcept;
        explicit FVertexTangentBuffer(const Rhi::FRhiBufferDesc& desc) noexcept;

        void               SetData(const void* data, u32 sizeBytes, u32 strideBytes);

        [[nodiscard]] auto GetBuffer() const noexcept -> Rhi::FRhiBufferRef { return mBuffer; }
        [[nodiscard]] auto GetView() const noexcept -> Rhi::FRhiVertexBufferView;

        [[nodiscard]] auto GetSizeBytes() const noexcept -> u64 { return mDesc.mSizeBytes; }
        [[nodiscard]] auto GetStrideBytes() const noexcept -> u32 { return mStrideBytes; }

        [[nodiscard]] auto GetElementCount() const noexcept -> u32 {
            if (mStrideBytes == 0U) {
                return 0U;
            }
            return static_cast<u32>(mDesc.mSizeBytes / static_cast<u64>(mStrideBytes));
        }

    protected:
        void InitRHI() override;
        void ReleaseRHI() override;
        void UpdateRHI() override;

    private:
        Rhi::FRhiBufferDesc mDesc{};
        u32                 mStrideBytes = 0U;
        TVector<u8>         mStagingData;
        Rhi::FRhiBufferRef  mBuffer;
    };

    class AE_RENDER_CORE_API FVertexUVBuffer : public FRenderResource {
    public:
        FVertexUVBuffer() noexcept;
        explicit FVertexUVBuffer(const Rhi::FRhiBufferDesc& desc) noexcept;

        void               SetData(const void* data, u32 sizeBytes, u32 strideBytes);

        [[nodiscard]] auto GetBuffer() const noexcept -> Rhi::FRhiBufferRef { return mBuffer; }
        [[nodiscard]] auto GetView() const noexcept -> Rhi::FRhiVertexBufferView;

        [[nodiscard]] auto GetSizeBytes() const noexcept -> u64 { return mDesc.mSizeBytes; }
        [[nodiscard]] auto GetStrideBytes() const noexcept -> u32 { return mStrideBytes; }

        [[nodiscard]] auto GetElementCount() const noexcept -> u32 {
            if (mStrideBytes == 0U) {
                return 0U;
            }
            return static_cast<u32>(mDesc.mSizeBytes / static_cast<u64>(mStrideBytes));
        }

    protected:
        void InitRHI() override;
        void ReleaseRHI() override;
        void UpdateRHI() override;

    private:
        Rhi::FRhiBufferDesc mDesc{};
        u32                 mStrideBytes = 0U;
        TVector<u8>         mStagingData;
        Rhi::FRhiBufferRef  mBuffer;
    };

    class AE_RENDER_CORE_API FIndexBuffer : public FRenderResource {
    public:
        FIndexBuffer() noexcept;
        explicit FIndexBuffer(const Rhi::FRhiBufferDesc& desc,
            Rhi::ERhiIndexType indexType = Rhi::ERhiIndexType::Uint32) noexcept;

        void               SetData(const void* data, u32 sizeBytes, Rhi::ERhiIndexType indexType);

        [[nodiscard]] auto GetBuffer() const noexcept -> Rhi::FRhiBufferRef { return mBuffer; }
        [[nodiscard]] auto GetView() const noexcept -> Rhi::FRhiIndexBufferView;
        [[nodiscard]] auto GetIndexType() const noexcept -> Rhi::ERhiIndexType {
            return mIndexType;
        }

        [[nodiscard]] auto GetSizeBytes() const noexcept -> u64 { return mDesc.mSizeBytes; }

    protected:
        void InitRHI() override;
        void ReleaseRHI() override;
        void UpdateRHI() override;

    private:
        Rhi::FRhiBufferDesc mDesc{};
        Rhi::ERhiIndexType  mIndexType = Rhi::ERhiIndexType::Uint32;
        TVector<u8>         mStagingData;
        Rhi::FRhiBufferRef  mBuffer;
    };

    class AE_RENDER_CORE_API FTexture : public FRenderResource {
    public:
        FTexture() noexcept = default;
        explicit FTexture(const Rhi::FRhiTextureDesc& desc) noexcept;

        void               SetDesc(const Rhi::FRhiTextureDesc& desc) noexcept;
        [[nodiscard]] auto GetDesc() const noexcept -> const Rhi::FRhiTextureDesc&;

        [[nodiscard]] auto GetTexture() const noexcept -> Rhi::FRhiTextureRef { return mTexture; }

    protected:
        void InitRHI() override;
        void ReleaseRHI() override;

    protected:
        Rhi::FRhiTextureDesc mDesc{};
        Rhi::FRhiTextureRef  mTexture;
    };

    class AE_RENDER_CORE_API FTextureWithSRV : public FTexture {
    public:
        FTextureWithSRV() noexcept = default;
        explicit FTextureWithSRV(const Rhi::FRhiTextureDesc& desc) noexcept;

        void               SetSRVDesc(const Rhi::FRhiShaderResourceViewDesc& desc) noexcept;
        [[nodiscard]] auto GetSRV() const noexcept -> Rhi::FRhiShaderResourceViewRef {
            return mSRV;
        }

    protected:
        void InitRHI() override;
        void ReleaseRHI() override;

    private:
        Rhi::FRhiShaderResourceViewDesc mSRVDesc{};
        Rhi::FRhiShaderResourceViewRef  mSRV;
    };

    class AE_RENDER_CORE_API FTextureWithUAV : public FTexture {
    public:
        FTextureWithUAV() noexcept;
        explicit FTextureWithUAV(const Rhi::FRhiTextureDesc& desc) noexcept;

        void               SetUAVDesc(const Rhi::FRhiUnorderedAccessViewDesc& desc) noexcept;
        [[nodiscard]] auto GetUAV() const noexcept -> Rhi::FRhiUnorderedAccessViewRef {
            return mUAV;
        }

    protected:
        void InitRHI() override;
        void ReleaseRHI() override;

    private:
        Rhi::FRhiUnorderedAccessViewDesc mUAVDesc{};
        Rhi::FRhiUnorderedAccessViewRef  mUAV;
    };

    class AE_RENDER_CORE_API FTextureWithSRVUAV : public FTexture {
    public:
        FTextureWithSRVUAV() noexcept;
        explicit FTextureWithSRVUAV(const Rhi::FRhiTextureDesc& desc) noexcept;

        void               SetSRVDesc(const Rhi::FRhiShaderResourceViewDesc& desc) noexcept;
        void               SetUAVDesc(const Rhi::FRhiUnorderedAccessViewDesc& desc) noexcept;

        [[nodiscard]] auto GetSRV() const noexcept -> Rhi::FRhiShaderResourceViewRef {
            return mSRV;
        }
        [[nodiscard]] auto GetUAV() const noexcept -> Rhi::FRhiUnorderedAccessViewRef {
            return mUAV;
        }

    protected:
        void InitRHI() override;
        void ReleaseRHI() override;

    private:
        Rhi::FRhiShaderResourceViewDesc  mSRVDesc{};
        Rhi::FRhiUnorderedAccessViewDesc mUAVDesc{};
        Rhi::FRhiShaderResourceViewRef   mSRV;
        Rhi::FRhiUnorderedAccessViewRef  mUAV;
    };

} // namespace AltinaEngine::RenderCore
