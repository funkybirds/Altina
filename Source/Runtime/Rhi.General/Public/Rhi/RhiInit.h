#pragma once

#include "Rhi/RhiContext.h"
#include "Rhi/RhiDevice.h"
#include "Rhi/RhiStructs.h"
#include "RhiGeneralAPI.h"

namespace AltinaEngine::Rhi {
    struct FRhiFrameStats {
        u64 mFrameIndex           = 0ULL;
        u64 mSetVertexBufferCalls = 0ULL;
    };

    AE_RHI_GENERAL_API auto RHIInit(FRhiContext& context, const FRhiInitDesc& initDesc,
        const FRhiDeviceDesc& deviceDesc   = FRhiDeviceDesc{},
        u32                   adapterIndex = kRhiInvalidAdapterIndex) -> TShared<FRhiDevice>;

    // Returns the backend requested at RHIInit(). This is what shader compilation should target.
    AE_RHI_GENERAL_API auto RHIGetBackend() noexcept -> ERhiBackend;

    AE_RHI_GENERAL_API auto RHIGetDevice() noexcept -> FRhiDevice*;
    AE_RHI_GENERAL_API auto RHICreateBuffer(const FRhiBufferDesc& desc) -> FRhiBufferRef;
    AE_RHI_GENERAL_API auto RHICreateTexture(const FRhiTextureDesc& desc) -> FRhiTextureRef;
    AE_RHI_GENERAL_API auto RHICreateViewport(const FRhiViewportDesc& desc) -> FRhiViewportRef;
    AE_RHI_GENERAL_API auto RHICreateSampler(const FRhiSamplerDesc& desc) -> FRhiSamplerRef;
    AE_RHI_GENERAL_API void RHIResetFrameStats(u64 frameIndex) noexcept;
    AE_RHI_GENERAL_API void RHIRecordSetVertexBufferCall(u64 count = 1ULL) noexcept;
    AE_RHI_GENERAL_API auto RHIGetFrameStats() noexcept -> FRhiFrameStats;

    AE_RHI_GENERAL_API void RHIExit(FRhiContext& context) noexcept;
} // namespace AltinaEngine::Rhi
