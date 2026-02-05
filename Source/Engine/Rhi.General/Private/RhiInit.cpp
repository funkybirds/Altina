#include "Rhi/RhiInit.h"

namespace AltinaEngine::Rhi {
    auto RHIInit(FRhiContext& context, const FRhiInitDesc& initDesc,
        const FRhiDeviceDesc& deviceDesc, u32 adapterIndex) -> TShared<FRhiDevice> {
        if (!context.Init(initDesc)) {
            return {};
        }

        TShared<FRhiDevice> device = context.CreateDevice(adapterIndex, deviceDesc);
        if (!device) {
            context.Shutdown();
            return {};
        }

        return device;
    }

    void RHIExit(FRhiContext& context) noexcept {
        context.Shutdown();
    }
} // namespace AltinaEngine::Rhi
