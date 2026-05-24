#include "Launch/RhiLaunchConfig.h"

#include "Utility/EngineConfig/EngineConfig.h"

namespace AltinaEngine::Launch {
    auto ResolveRhiInitDesc(const Core::Utility::EngineConfig::FConfigCollection& config,
        Rhi::ERhiBackend backend) -> Rhi::FRhiInitDesc {
        const bool enableGpuValidation = config.GetBool(TEXT("Rhi/EnableGpuValidation"));
        const bool enableValidation =
            enableGpuValidation || config.GetBool(TEXT("Rhi/EnableValidation"));

        Rhi::FRhiInitDesc desc{};
        desc.mAppName.Assign(TEXT("AltinaEngine"));
        desc.mBackend             = backend;
        desc.mEnableValidation    = enableValidation;
        desc.mEnableGpuValidation = enableGpuValidation;
        desc.mEnableDebugNames    = config.GetBool(TEXT("Rhi/EnableDebugNames"));
        return desc;
    }

    auto ResolveRhiDeviceDesc(const Rhi::FRhiInitDesc& initDesc) noexcept -> Rhi::FRhiDeviceDesc {
        Rhi::FRhiDeviceDesc desc{};
        desc.mEnableValidation    = initDesc.mEnableValidation;
        desc.mEnableGpuValidation = initDesc.mEnableGpuValidation;
        return desc;
    }
} // namespace AltinaEngine::Launch
