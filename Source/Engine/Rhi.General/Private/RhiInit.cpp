#include "Rhi/RhiInit.h"

#include "Rhi/RhiBuffer.h"
#include "Rhi/RhiSampler.h"
#include "Rhi/RhiTexture.h"

namespace AltinaEngine::Rhi {
    namespace {
        TShared<FRhiDevice> gRhiDevice;
    }

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

        gRhiDevice = device;
        return device;
    }

    auto RHIGetDevice() noexcept -> FRhiDevice* {
        return gRhiDevice.Get();
    }

    auto RHICreateBuffer(const FRhiBufferDesc& desc) -> FRhiBufferRef {
        FRhiDevice* device = RHIGetDevice();
        return device ? device->CreateBuffer(desc) : FRhiBufferRef{};
    }

    auto RHICreateTexture(const FRhiTextureDesc& desc) -> FRhiTextureRef {
        FRhiDevice* device = RHIGetDevice();
        return device ? device->CreateTexture(desc) : FRhiTextureRef{};
    }

    auto RHICreateSampler(const FRhiSamplerDesc& desc) -> FRhiSamplerRef {
        FRhiDevice* device = RHIGetDevice();
        return device ? device->CreateSampler(desc) : FRhiSamplerRef{};
    }

    void RHIExit(FRhiContext& context) noexcept {
        gRhiDevice.Reset();
        context.Shutdown();
    }
} // namespace AltinaEngine::Rhi
