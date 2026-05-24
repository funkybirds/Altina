#pragma once

#include "Base/LaunchAPI.h"
#include "Rhi/RhiStructs.h"

namespace AltinaEngine::Core::Utility::EngineConfig {
    class FConfigCollection;
} // namespace AltinaEngine::Core::Utility::EngineConfig

namespace AltinaEngine::Launch {
    AE_LAUNCH_API auto ResolveRhiInitDesc(
        const Core::Utility::EngineConfig::FConfigCollection& config, Rhi::ERhiBackend backend)
        -> Rhi::FRhiInitDesc;

    AE_LAUNCH_API auto ResolveRhiDeviceDesc(const Rhi::FRhiInitDesc& initDesc) noexcept
        -> Rhi::FRhiDeviceDesc;
} // namespace AltinaEngine::Launch
