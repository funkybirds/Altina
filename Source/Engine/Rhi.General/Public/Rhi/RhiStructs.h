#pragma once

#include "Rhi/RhiEnums.h"
#include "Rhi/RhiFwd.h"
#include "Shader/ShaderReflection.h"
#include "Shader/ShaderTypes.h"
#include "Container/String.h"
#include "Container/StringView.h"
#include "Container/Vector.h"

namespace AltinaEngine::Rhi {
    namespace Container = Core::Container;
    using Container::FString;
    using Container::FStringView;
    using Container::TVector;
    using Shader::EShaderStage;
    using Shader::FShaderBytecode;
    using Shader::FShaderReflection;

    inline constexpr u32 kRhiInvalidAdapterIndex = 0xFFFFFFFFu;
    inline constexpr u64 kRhiUnknownMemoryBytes  = 0ULL;
    inline constexpr u32 kRhiLimitUnknown        = 0U;

    struct FRhiInitDesc {
        FString           mAppName;
        u32               mAppVersion          = 1U;
        u32               mEngineVersion       = 1U;
        ERhiBackend       mBackend             = ERhiBackend::Unknown;
        ERhiGpuPreference mAdapterPreference   = ERhiGpuPreference::Auto;
        bool              mEnableValidation    = false;
        bool              mEnableGpuValidation = false;
        bool              mEnableDebugLayer    = false;
        bool              mEnableDebugNames    = true;
    };

    struct FRhiAdapterDesc {
        FString            mName;
        ERhiVendorId       mVendorId                   = ERhiVendorId::Unknown;
        u32                mDeviceId                   = 0U;
        ERhiAdapterType    mType                       = ERhiAdapterType::Unknown;
        u64                mDedicatedVideoMemoryBytes  = kRhiUnknownMemoryBytes;
        u64                mDedicatedSystemMemoryBytes = kRhiUnknownMemoryBytes;
        u64                mSharedSystemMemoryBytes    = kRhiUnknownMemoryBytes;
        u32                mDriverVersion              = 0U;
        u32                mApiVersion                 = 0U;

        [[nodiscard]] auto GetName() const noexcept -> FStringView { return mName.ToView(); }

        [[nodiscard]] auto IsValid() const noexcept -> bool {
            return !mName.IsEmptyString() || (mVendorId != ERhiVendorId::Unknown);
        }

        [[nodiscard]] auto IsIntegrated() const noexcept -> bool {
            return mType == ERhiAdapterType::Integrated;
        }

        [[nodiscard]] auto IsDiscrete() const noexcept -> bool {
            return mType == ERhiAdapterType::Discrete;
        }

        [[nodiscard]] auto IsSoftware() const noexcept -> bool {
            return (mType == ERhiAdapterType::Software) || (mType == ERhiAdapterType::Cpu);
        }

        [[nodiscard]] auto GetTotalLocalMemoryBytes() const noexcept -> u64 {
            return mDedicatedVideoMemoryBytes + mDedicatedSystemMemoryBytes;
        }

        [[nodiscard]] auto GetTotalMemoryBytes() const noexcept -> u64 {
            return GetTotalLocalMemoryBytes() + mSharedSystemMemoryBytes;
        }
    };

    struct FRhiSupportedFeatures {
        bool               mBindless            = false;
        bool               mRayTracing          = false;
        bool               mMeshShaders         = false;
        bool               mBarycentrics        = false;
        bool               mVariableRateShading = false;
        bool               mSamplerFeedback     = false;
        bool               mTimelineSemaphore   = false;

        [[nodiscard]] auto IsSupported(ERhiFeature feature) const noexcept -> bool {
            switch (feature) {
                case ERhiFeature::Bindless:
                    return mBindless;
                case ERhiFeature::RayTracing:
                    return mRayTracing;
                case ERhiFeature::MeshShaders:
                    return mMeshShaders;
                case ERhiFeature::Barycentrics:
                    return mBarycentrics;
                case ERhiFeature::VariableRateShading:
                    return mVariableRateShading;
                case ERhiFeature::SamplerFeedback:
                    return mSamplerFeedback;
                case ERhiFeature::TimelineSemaphore:
                    return mTimelineSemaphore;
                default:
                    return false;
            }
        }
    };

    struct FRhiSupportedLimits {
        u64 mMaxBufferSize                  = kRhiUnknownMemoryBytes;
        u32 mMaxTextureDimension1D          = kRhiLimitUnknown;
        u32 mMaxTextureDimension2D          = kRhiLimitUnknown;
        u32 mMaxTextureDimension3D          = kRhiLimitUnknown;
        u32 mMaxTextureArrayLayers          = kRhiLimitUnknown;
        u32 mMaxSamplers                    = kRhiLimitUnknown;
        u32 mMaxBindGroups                  = kRhiLimitUnknown;
        u32 mMaxColorAttachments            = kRhiLimitUnknown;
        u32 mMaxComputeWorkgroupSizeX       = kRhiLimitUnknown;
        u32 mMaxComputeWorkgroupSizeY       = kRhiLimitUnknown;
        u32 mMaxComputeWorkgroupSizeZ       = kRhiLimitUnknown;
        u32 mMaxComputeWorkgroupInvocations = kRhiLimitUnknown;
    };

    struct FRhiQueueCapabilities {
        bool mSupportsGraphics     = false;
        bool mSupportsCompute      = false;
        bool mSupportsCopy         = false;
        bool mSupportsAsyncCompute = false;
        bool mSupportsAsyncCopy    = false;
    };

    struct FRhiDeviceDesc {
        FString mDebugName;
        bool    mEnableDebugLayer       = false;
        bool    mEnableGpuValidation    = false;
        bool    mEnableStablePowerState = false;
    };

    struct FRhiBufferDesc {
        FString             mDebugName;
        u64                 mSizeBytes = 0ULL;
        ERhiResourceUsage   mUsage     = ERhiResourceUsage::Default;
        ERhiBufferBindFlags mBindFlags = ERhiBufferBindFlags::None;
        ERhiCpuAccess       mCpuAccess = ERhiCpuAccess::None;
    };

    struct FRhiTextureDesc {
        FString              mDebugName;
        u32                  mWidth       = 0U;
        u32                  mHeight      = 0U;
        u32                  mDepth       = 1U;
        u32                  mMipLevels   = 1U;
        u32                  mArrayLayers = 1U;
        u32                  mSampleCount = 1U;
        ERhiFormat           mFormat      = ERhiFormat::R8G8B8A8Unorm;
        ERhiResourceUsage    mUsage       = ERhiResourceUsage::Default;
        ERhiTextureBindFlags mBindFlags   = ERhiTextureBindFlags::ShaderResource;
        ERhiCpuAccess        mCpuAccess   = ERhiCpuAccess::None;
    };

    struct FRhiTextureViewRange {
        u32 mBaseMip         = 0U;
        u32 mMipCount        = 0U;
        u32 mBaseArrayLayer  = 0U;
        u32 mLayerCount      = 0U;
        u32 mBaseDepthSlice  = 0U;
        u32 mDepthSliceCount = 0U;
    };

    struct FRhiBufferViewRange {
        u64 mOffsetBytes = 0ULL;
        u64 mSizeBytes   = 0ULL;
    };

    struct FRhiShaderResourceViewDesc {
        FString              mDebugName;
        FRhiTexture*         mTexture = nullptr;
        FRhiBuffer*          mBuffer  = nullptr;
        ERhiFormat           mFormat  = ERhiFormat::Unknown;
        FRhiTextureViewRange mTextureRange;
        FRhiBufferViewRange  mBufferRange;
    };

    struct FRhiUnorderedAccessViewDesc {
        FString              mDebugName;
        FRhiTexture*         mTexture = nullptr;
        FRhiBuffer*          mBuffer  = nullptr;
        ERhiFormat           mFormat  = ERhiFormat::Unknown;
        FRhiTextureViewRange mTextureRange;
        FRhiBufferViewRange  mBufferRange;
    };

    struct FRhiRenderTargetViewDesc {
        FString              mDebugName;
        FRhiTexture*         mTexture = nullptr;
        ERhiFormat           mFormat  = ERhiFormat::Unknown;
        FRhiTextureViewRange mRange;
    };

    struct FRhiDepthStencilViewDesc {
        FString              mDebugName;
        FRhiTexture*         mTexture = nullptr;
        ERhiFormat           mFormat  = ERhiFormat::Unknown;
        FRhiTextureViewRange mRange;
        bool                 mReadOnlyDepth   = false;
        bool                 mReadOnlyStencil = false;
    };

    struct FRhiViewportDesc {
        FString    mDebugName;
        u32        mWidth        = 0U;
        u32        mHeight       = 0U;
        ERhiFormat mFormat       = ERhiFormat::B8G8R8A8Unorm;
        u32        mBufferCount  = 2U;
        bool       mAllowTearing = false;
        void*      mNativeHandle = nullptr;
    };

    struct FRhiVertexBufferView {
        FRhiBuffer* mBuffer      = nullptr;
        u32         mStrideBytes = 0U;
        u32         mOffsetBytes = 0U;
    };

    struct FRhiIndexBufferView {
        FRhiBuffer*   mBuffer      = nullptr;
        ERhiIndexType mIndexType   = ERhiIndexType::Uint32;
        u32           mOffsetBytes = 0U;
    };

    struct FRhiViewportRect {
        f32 mX        = 0.0f;
        f32 mY        = 0.0f;
        f32 mWidth    = 0.0f;
        f32 mHeight   = 0.0f;
        f32 mMinDepth = 0.0f;
        f32 mMaxDepth = 1.0f;
    };

    struct FRhiScissorRect {
        i32 mX      = 0;
        i32 mY      = 0;
        u32 mWidth  = 0U;
        u32 mHeight = 0U;
    };

    struct FRhiClearColor {
        f32 mR = 0.0f;
        f32 mG = 0.0f;
        f32 mB = 0.0f;
        f32 mA = 1.0f;
    };

    struct FRhiClearDepthStencil {
        f32 mDepth   = 1.0f;
        u32 mStencil = 0U;
    };

    struct FRhiSamplerDesc {
        FString mDebugName;
    };

    struct FRhiShaderDesc {
        FString           mDebugName;
        EShaderStage      mStage = EShaderStage::Vertex;
        FShaderBytecode   mBytecode;
        FShaderReflection mReflection;
    };

    struct FRhiVertexAttributeDesc {
        FString    mSemanticName;
        u32        mSemanticIndex     = 0U;
        ERhiFormat mFormat            = ERhiFormat::R32Float;
        u32        mInputSlot         = 0U;
        u32        mAlignedByteOffset = 0U;
        bool       mPerInstance       = false;
        u32        mInstanceStepRate  = 0U;
    };

    struct FRhiVertexLayoutDesc {
        TVector<FRhiVertexAttributeDesc> mAttributes;
    };

    struct FRhiRasterStateDesc {
        ERhiRasterFillMode  mFillMode            = ERhiRasterFillMode::Solid;
        ERhiRasterCullMode  mCullMode            = ERhiRasterCullMode::Back;
        ERhiRasterFrontFace mFrontFace           = ERhiRasterFrontFace::CCW;
        i32                 mDepthBias           = 0;
        f32                 mDepthBiasClamp      = 0.0f;
        f32                 mSlopeScaledDepthBias = 0.0f;
        bool                mDepthClip           = true;
        bool                mConservativeRaster  = false;
    };

    struct FRhiDepthStateDesc {
        bool          mDepthEnable = true;
        bool          mDepthWrite  = true;
        ERhiCompareOp mDepthCompare = ERhiCompareOp::LessEqual;
    };

    struct FRhiBlendStateDesc {
        bool             mBlendEnable    = false;
        ERhiBlendFactor  mSrcColor       = ERhiBlendFactor::One;
        ERhiBlendFactor  mDstColor       = ERhiBlendFactor::Zero;
        ERhiBlendOp      mColorOp        = ERhiBlendOp::Add;
        ERhiBlendFactor  mSrcAlpha       = ERhiBlendFactor::One;
        ERhiBlendFactor  mDstAlpha       = ERhiBlendFactor::Zero;
        ERhiBlendOp      mAlphaOp        = ERhiBlendOp::Add;
        ERhiColorWriteMask mColorWriteMask = ERhiColorWriteMask::All;
    };

    struct FRhiGraphicsPipelineDesc {
        FString              mDebugName;
        FRhiPipelineLayout*  mPipelineLayout = nullptr;
        FRhiShader*          mVertexShader   = nullptr;
        FRhiShader*          mPixelShader    = nullptr;
        FRhiShader*          mGeometryShader = nullptr;
        FRhiShader*          mHullShader     = nullptr;
        FRhiShader*          mDomainShader   = nullptr;
        FRhiVertexLayoutDesc mVertexLayout;
    };

    struct FRhiComputePipelineDesc {
        FString             mDebugName;
        FRhiPipelineLayout* mPipelineLayout = nullptr;
        FRhiShader*         mComputeShader  = nullptr;
    };

    struct FRhiPushConstantRange {
        u32                  mOffset     = 0U;
        u32                  mSize       = 0U;
        ERhiShaderStageFlags mVisibility = ERhiShaderStageFlags::All;
    };

    struct FRhiBindGroupLayoutEntry {
        u32                  mBinding          = 0U;
        ERhiBindingType      mType             = ERhiBindingType::ConstantBuffer;
        ERhiShaderStageFlags mVisibility       = ERhiShaderStageFlags::All;
        u32                  mArrayCount       = 1U;
        bool                 mHasDynamicOffset = false;
    };

    struct FRhiBindGroupLayoutDesc {
        FString                           mDebugName;
        u32                               mSetIndex = 0U;
        TVector<FRhiBindGroupLayoutEntry> mEntries;
        u64                               mLayoutHash = 0ULL;
    };

    struct FRhiBindGroupEntry {
        u32             mBinding    = 0U;
        ERhiBindingType mType       = ERhiBindingType::ConstantBuffer;
        FRhiBuffer*     mBuffer     = nullptr;
        FRhiTexture*    mTexture    = nullptr;
        FRhiSampler*    mSampler    = nullptr;
        u64             mOffset     = 0ULL;
        u64             mSize       = 0ULL;
        u32             mArrayIndex = 0U;
    };

    struct FRhiBindGroupDesc {
        FString                     mDebugName;
        FRhiBindGroupLayout*        mLayout = nullptr;
        TVector<FRhiBindGroupEntry> mEntries;
    };

    struct FRhiPipelineLayoutDesc {
        FString                        mDebugName;
        TVector<FRhiBindGroupLayout*>  mBindGroupLayouts;
        TVector<FRhiPushConstantRange> mPushConstants;
        u64                            mLayoutHash = 0ULL;
    };

    struct FRhiCommandPoolDesc {
        FString       mDebugName;
        ERhiQueueType mQueueType = ERhiQueueType::Graphics;
    };

    struct FRhiCommandListDesc {
        FString             mDebugName;
        ERhiQueueType       mQueueType = ERhiQueueType::Graphics;
        ERhiCommandListType mListType  = ERhiCommandListType::Direct;
    };

    struct FRhiCommandContextDesc {
        FString             mDebugName;
        ERhiQueueType       mQueueType = ERhiQueueType::Graphics;
        ERhiCommandListType mListType  = ERhiCommandListType::Direct;
    };

    struct FRhiQueueWait {
        FRhiSemaphore* mSemaphore = nullptr;
        u64            mValue     = 0ULL;
    };

    struct FRhiQueueSignal {
        FRhiSemaphore* mSemaphore = nullptr;
        u64            mValue     = 0ULL;
    };

    struct FRhiSubmitInfo {
        FRhiCommandList* const* mCommandLists     = nullptr;
        u32                     mCommandListCount = 0U;

        const FRhiQueueWait*    mWaits     = nullptr;
        u32                     mWaitCount = 0U;

        const FRhiQueueSignal*  mSignals     = nullptr;
        u32                     mSignalCount = 0U;

        FRhiFence*              mFence      = nullptr;
        u64                     mFenceValue = 0ULL;
    };

    struct FRhiPresentInfo {
        FRhiViewport* mViewport     = nullptr;
        u32           mSyncInterval = 1U;
        u32           mFlags        = 0U;
    };

    struct FRhiTransitionInfo {
        FRhiResource*        mResource = nullptr;
        ERhiResourceState    mBefore   = ERhiResourceState::Unknown;
        ERhiResourceState    mAfter    = ERhiResourceState::Unknown;
        FRhiTextureViewRange mTextureRange;
        FRhiBufferViewRange  mBufferRange;
    };

    struct FRhiTransitionCreateInfo {
        const FRhiTransitionInfo* mTransitions     = nullptr;
        u32                       mTransitionCount = 0U;
        ERhiQueueType             mSrcQueue        = ERhiQueueType::Graphics;
        ERhiQueueType             mDstQueue        = ERhiQueueType::Graphics;
        u32                       mFlags           = 0U;
    };

    struct FRhiRenderPassColorAttachment {
        FRhiRenderTargetView* mView    = nullptr;
        ERhiLoadOp            mLoadOp  = ERhiLoadOp::Clear;
        ERhiStoreOp           mStoreOp = ERhiStoreOp::Store;
        FRhiClearColor        mClearColor;
    };

    struct FRhiRenderPassDepthStencilAttachment {
        FRhiDepthStencilView* mView           = nullptr;
        ERhiLoadOp            mDepthLoadOp    = ERhiLoadOp::Clear;
        ERhiStoreOp           mDepthStoreOp   = ERhiStoreOp::Store;
        ERhiLoadOp            mStencilLoadOp  = ERhiLoadOp::Clear;
        ERhiStoreOp           mStencilStoreOp = ERhiStoreOp::Store;
        FRhiClearDepthStencil mClearDepthStencil;
        bool                  mReadOnlyDepth   = false;
        bool                  mReadOnlyStencil = false;
    };

    struct FRhiRenderPassDesc {
        FString                                     mDebugName;
        u32                                         mColorAttachmentCount   = 0U;
        const FRhiRenderPassColorAttachment*        mColorAttachments       = nullptr;
        const FRhiRenderPassDepthStencilAttachment* mDepthStencilAttachment = nullptr;
    };

} // namespace AltinaEngine::Rhi
