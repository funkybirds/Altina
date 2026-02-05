#pragma once

#include "Types/Aliases.h"
#include "Types/Traits.h"

namespace AltinaEngine::Rhi {

    enum class ERhiBackend : u8 {
        Unknown = 0,
        DirectX12,
        Vulkan,
        DirectX11,
        OpenGL,
        Metal
    };

    enum class ERhiAdapterType : u8 {
        Unknown = 0,
        Integrated,
        Discrete,
        Virtual,
        Software,
        Cpu
    };

    enum class ERhiGpuPreference : u8 {
        Auto = 0,
        HighPerformance,
        LowPower
    };

    enum class ERhiQueueType : u8 {
        Graphics = 0,
        Compute,
        Copy
    };

    enum class ERhiVendorId : u32 {
        Unknown   = 0,
        Nvidia    = 0x10DE,
        AMD       = 0x1002,
        Intel     = 0x8086,
        Microsoft = 0x1414
    };

    enum class ERhiFeature : u8 {
        Bindless = 0,
        RayTracing,
        MeshShaders,
        Barycentrics,
        VariableRateShading,
        SamplerFeedback,
        TimelineSemaphore
    };

    template <typename T>
        requires AltinaEngine::CEnum<T>
    [[nodiscard]] constexpr auto ToUnderlying(T value) noexcept -> AltinaEngine::TUnderlyingType<T> {
        return static_cast<AltinaEngine::TUnderlyingType<T>>(value);
    }

} // namespace AltinaEngine::Rhi
