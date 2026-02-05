#pragma once

#include "Rhi/RhiContext.h"
#include "Rhi/RhiDevice.h"
#include "Rhi/RhiStructs.h"
#include "RhiGeneralAPI.h"

namespace AltinaEngine::Rhi {
    AE_RHI_GENERAL_API auto RHIInit(FRhiContext& context, const FRhiInitDesc& initDesc,
        const FRhiDeviceDesc& deviceDesc = FRhiDeviceDesc{},
        u32 adapterIndex = kRhiInvalidAdapterIndex) -> TShared<FRhiDevice>;

    AE_RHI_GENERAL_API void RHIExit(FRhiContext& context) noexcept;
} // namespace AltinaEngine::Rhi
