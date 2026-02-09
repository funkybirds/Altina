#pragma once

#include "Rhi/RhiContext.h"
#include "Rhi/RhiDevice.h"
#include "Rhi/RhiStructs.h"
#include "RhiGeneralAPI.h"

namespace AltinaEngine::Rhi {
    AE_RHI_GENERAL_API auto RHIInit(FRhiContext& context, const FRhiInitDesc& initDesc,
        const FRhiDeviceDesc& deviceDesc = FRhiDeviceDesc{},
        u32 adapterIndex = kRhiInvalidAdapterIndex) -> TShared<FRhiDevice>;

    AE_RHI_GENERAL_API auto RHIGetDevice() noexcept -> FRhiDevice*;
    AE_RHI_GENERAL_API auto RHICreateBuffer(const FRhiBufferDesc& desc) -> FRhiBufferRef;
    AE_RHI_GENERAL_API auto RHICreateTexture(const FRhiTextureDesc& desc) -> FRhiTextureRef;
    AE_RHI_GENERAL_API auto RHICreateViewport(const FRhiViewportDesc& desc) -> FRhiViewportRef;
    AE_RHI_GENERAL_API auto RHICreateSampler(const FRhiSamplerDesc& desc) -> FRhiSamplerRef;

    AE_RHI_GENERAL_API void RHIExit(FRhiContext& context) noexcept;
} // namespace AltinaEngine::Rhi
