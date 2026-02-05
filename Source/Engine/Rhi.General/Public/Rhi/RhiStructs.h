#pragma once

#include "Rhi/RhiEnums.h"
#include "Container/String.h"
#include "Container/StringView.h"

namespace AltinaEngine::Rhi {
    using Core::Container::FString;
    using Core::Container::FStringView;

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
        FString         mName;
        ERhiVendorId    mVendorId                   = ERhiVendorId::Unknown;
        u32             mDeviceId                   = 0U;
        ERhiAdapterType mType                       = ERhiAdapterType::Unknown;
        u64             mDedicatedVideoMemoryBytes  = kRhiUnknownMemoryBytes;
        u64             mDedicatedSystemMemoryBytes = kRhiUnknownMemoryBytes;
        u64             mSharedSystemMemoryBytes    = kRhiUnknownMemoryBytes;
        u32             mDriverVersion              = 0U;
        u32             mApiVersion                 = 0U;

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
        bool mBindless            = false;
        bool mRayTracing          = false;
        bool mMeshShaders         = false;
        bool mBarycentrics        = false;
        bool mVariableRateShading = false;
        bool mSamplerFeedback     = false;
        bool mTimelineSemaphore   = false;

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
        u64 mMaxBufferSize                 = kRhiUnknownMemoryBytes;
        u32 mMaxTextureDimension1D         = kRhiLimitUnknown;
        u32 mMaxTextureDimension2D         = kRhiLimitUnknown;
        u32 mMaxTextureDimension3D         = kRhiLimitUnknown;
        u32 mMaxTextureArrayLayers         = kRhiLimitUnknown;
        u32 mMaxSamplers                   = kRhiLimitUnknown;
        u32 mMaxBindGroups                 = kRhiLimitUnknown;
        u32 mMaxColorAttachments           = kRhiLimitUnknown;
        u32 mMaxComputeWorkgroupSizeX      = kRhiLimitUnknown;
        u32 mMaxComputeWorkgroupSizeY      = kRhiLimitUnknown;
        u32 mMaxComputeWorkgroupSizeZ      = kRhiLimitUnknown;
        u32 mMaxComputeWorkgroupInvocations = kRhiLimitUnknown;
    };

    struct FRhiDeviceDesc {
        FString mDebugName;
        bool    mEnableDebugLayer     = false;
        bool    mEnableGpuValidation  = false;
        bool    mEnableStablePowerState = false;
    };

    struct FRhiBufferDesc {
        FString mDebugName;
        u64     mSizeBytes = 0ULL;
        ERhiResourceUsage   mUsage     = ERhiResourceUsage::Default;
        ERhiBufferBindFlags mBindFlags = ERhiBufferBindFlags::None;
        ERhiCpuAccess       mCpuAccess = ERhiCpuAccess::None;
    };

    struct FRhiTextureDesc {
        FString mDebugName;
        u32     mWidth     = 0U;
        u32     mHeight    = 0U;
        u32     mDepth     = 1U;
        u32     mMipLevels = 1U;
        u32     mArrayLayers = 1U;
        u32     mSampleCount = 1U;
        ERhiFormat          mFormat    = ERhiFormat::R8G8B8A8_UNORM;
        ERhiResourceUsage   mUsage     = ERhiResourceUsage::Default;
        ERhiTextureBindFlags mBindFlags = ERhiTextureBindFlags::ShaderResource;
        ERhiCpuAccess       mCpuAccess = ERhiCpuAccess::None;
    };

    struct FRhiSamplerDesc {
        FString mDebugName;
    };

    struct FRhiShaderDesc {
        FString mDebugName;
    };

    struct FRhiGraphicsPipelineDesc {
        FString mDebugName;
    };

    struct FRhiComputePipelineDesc {
        FString mDebugName;
    };

    struct FRhiPipelineLayoutDesc {
        FString mDebugName;
    };

    struct FRhiBindGroupLayoutDesc {
        FString mDebugName;
    };

    struct FRhiBindGroupDesc {
        FString mDebugName;
    };

    struct FRhiCommandPoolDesc {
        FString       mDebugName;
        ERhiQueueType mQueueType = ERhiQueueType::Graphics;
    };

    struct FRhiSubmitInfo {
    };

    struct FRhiPresentInfo {
    };

} // namespace AltinaEngine::Rhi
