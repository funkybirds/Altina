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

    enum class ERhiFormat : u16 {
        Unknown = 0,
        R8G8B8A8_UNORM,
        R8G8B8A8_UNORM_SRGB,
        B8G8R8A8_UNORM,
        B8G8R8A8_UNORM_SRGB,
        R16G16B16A16_FLOAT,
        R32_FLOAT,
        D24_UNORM_S8_UINT,
        D32_FLOAT
    };

    enum class ERhiResourceUsage : u8 {
        Default = 0,
        Immutable,
        Dynamic,
        Staging
    };

    enum class ERhiCpuAccess : u8 {
        None  = 0,
        Read  = 1u << 0,
        Write = 1u << 1
    };

    enum class ERhiBufferBindFlags : u32 {
        None           = 0,
        Vertex         = 1u << 0,
        Index          = 1u << 1,
        Constant       = 1u << 2,
        ShaderResource = 1u << 3,
        UnorderedAccess = 1u << 4,
        Indirect       = 1u << 5,
        CopySrc        = 1u << 6,
        CopyDst        = 1u << 7
    };

    enum class ERhiTextureBindFlags : u32 {
        None           = 0,
        ShaderResource = 1u << 0,
        RenderTarget   = 1u << 1,
        DepthStencil   = 1u << 2,
        UnorderedAccess = 1u << 3,
        CopySrc        = 1u << 4,
        CopyDst        = 1u << 5
    };

    enum class ERhiShaderStageFlags : u32 {
        None          = 0,
        Vertex        = 1u << 0,
        Pixel         = 1u << 1,
        Compute       = 1u << 2,
        Geometry      = 1u << 3,
        Hull          = 1u << 4,
        Domain        = 1u << 5,
        Mesh          = 1u << 6,
        Amplification = 1u << 7,
        AllGraphics   = (1u << 0) | (1u << 1) | (1u << 3) | (1u << 4) | (1u << 5)
                       | (1u << 6) | (1u << 7),
        All           = (1u << 0) | (1u << 1) | (1u << 2) | (1u << 3) | (1u << 4)
                       | (1u << 5) | (1u << 6) | (1u << 7)
    };

    enum class ERhiBindingType : u8 {
        ConstantBuffer = 0,
        SampledTexture,
        StorageTexture,
        SampledBuffer,
        StorageBuffer,
        Sampler,
        AccelerationStructure
    };

    template <typename T>
        requires AltinaEngine::CEnum<T>
    [[nodiscard]] constexpr auto ToUnderlying(T value) noexcept -> AltinaEngine::TUnderlyingType<T> {
        return static_cast<AltinaEngine::TUnderlyingType<T>>(value);
    }

    [[nodiscard]] constexpr auto operator|(ERhiCpuAccess lhs, ERhiCpuAccess rhs) noexcept
        -> ERhiCpuAccess {
        return static_cast<ERhiCpuAccess>(ToUnderlying(lhs) | ToUnderlying(rhs));
    }

    [[nodiscard]] constexpr auto operator&(ERhiCpuAccess lhs, ERhiCpuAccess rhs) noexcept
        -> ERhiCpuAccess {
        return static_cast<ERhiCpuAccess>(ToUnderlying(lhs) & ToUnderlying(rhs));
    }

    constexpr auto operator|=(ERhiCpuAccess& lhs, ERhiCpuAccess rhs) noexcept -> ERhiCpuAccess& {
        lhs = lhs | rhs;
        return lhs;
    }

    [[nodiscard]] constexpr auto operator|(ERhiBufferBindFlags lhs, ERhiBufferBindFlags rhs) noexcept
        -> ERhiBufferBindFlags {
        return static_cast<ERhiBufferBindFlags>(ToUnderlying(lhs) | ToUnderlying(rhs));
    }

    [[nodiscard]] constexpr auto operator&(ERhiBufferBindFlags lhs, ERhiBufferBindFlags rhs) noexcept
        -> ERhiBufferBindFlags {
        return static_cast<ERhiBufferBindFlags>(ToUnderlying(lhs) & ToUnderlying(rhs));
    }

    constexpr auto operator|=(ERhiBufferBindFlags& lhs, ERhiBufferBindFlags rhs) noexcept
        -> ERhiBufferBindFlags& {
        lhs = lhs | rhs;
        return lhs;
    }

    [[nodiscard]] constexpr auto operator|(ERhiTextureBindFlags lhs, ERhiTextureBindFlags rhs) noexcept
        -> ERhiTextureBindFlags {
        return static_cast<ERhiTextureBindFlags>(ToUnderlying(lhs) | ToUnderlying(rhs));
    }

    [[nodiscard]] constexpr auto operator&(ERhiTextureBindFlags lhs, ERhiTextureBindFlags rhs) noexcept
        -> ERhiTextureBindFlags {
        return static_cast<ERhiTextureBindFlags>(ToUnderlying(lhs) & ToUnderlying(rhs));
    }

    constexpr auto operator|=(ERhiTextureBindFlags& lhs, ERhiTextureBindFlags rhs) noexcept
        -> ERhiTextureBindFlags& {
        lhs = lhs | rhs;
        return lhs;
    }

    [[nodiscard]] constexpr auto operator|(ERhiShaderStageFlags lhs, ERhiShaderStageFlags rhs) noexcept
        -> ERhiShaderStageFlags {
        return static_cast<ERhiShaderStageFlags>(ToUnderlying(lhs) | ToUnderlying(rhs));
    }

    [[nodiscard]] constexpr auto operator&(ERhiShaderStageFlags lhs, ERhiShaderStageFlags rhs) noexcept
        -> ERhiShaderStageFlags {
        return static_cast<ERhiShaderStageFlags>(ToUnderlying(lhs) & ToUnderlying(rhs));
    }

    constexpr auto operator|=(ERhiShaderStageFlags& lhs, ERhiShaderStageFlags rhs) noexcept
        -> ERhiShaderStageFlags& {
        lhs = lhs | rhs;
        return lhs;
    }

    constexpr auto operator&=(ERhiShaderStageFlags& lhs, ERhiShaderStageFlags rhs) noexcept
        -> ERhiShaderStageFlags& {
        lhs = lhs & rhs;
        return lhs;
    }

    template <typename T>
        requires AltinaEngine::CEnum<T>
    [[nodiscard]] constexpr auto HasAnyFlags(T value, T flags) noexcept -> bool {
        return (ToUnderlying(value) & ToUnderlying(flags)) != 0;
    }

} // namespace AltinaEngine::Rhi
