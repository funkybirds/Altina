#pragma once

#include "Rhi/RhiEnums.h"
#include "Rhi/RhiStructs.h"
#include "Logging/Log.h"
#include "Types/Traits.h"

#if defined(AE_RHI_VULKAN_AVAILABLE) && AE_RHI_VULKAN_AVAILABLE
    #if AE_PLATFORM_WIN
        #ifdef TEXT
            #undef TEXT
        #endif
        #ifndef WIN32_LEAN_AND_MEAN
            #define WIN32_LEAN_AND_MEAN
        #endif
        #ifndef NOMINMAX
            #define NOMINMAX
        #endif
        #include <Windows.h>
        #ifdef CreateSemaphore
            #undef CreateSemaphore
        #endif
        #include <vulkan/vulkan_win32.h>
    #endif
    #include <vulkan/vulkan.h>
#endif

namespace AltinaEngine::Rhi::Vulkan::Detail {
    using AltinaEngine::Rhi::ERhiBlendFactor;
    using AltinaEngine::Rhi::ERhiBlendOp;
    using AltinaEngine::Rhi::ERhiBufferBindFlags;
    using AltinaEngine::Rhi::ERhiCompareOp;
    using AltinaEngine::Rhi::ERhiFormat;
    using AltinaEngine::Rhi::ERhiPrimitiveTopology;
    using AltinaEngine::Rhi::ERhiRasterCullMode;
    using AltinaEngine::Rhi::ERhiRasterFrontFace;
    using AltinaEngine::Rhi::ERhiResourceState;
    using AltinaEngine::Rhi::ERhiTextureBindFlags;

#if defined(AE_RHI_VULKAN_AVAILABLE) && AE_RHI_VULKAN_AVAILABLE
    [[nodiscard]] VkFormat            ToVkFormat(ERhiFormat format) noexcept;
    [[nodiscard]] VkImageAspectFlags  ToVkAspectFlags(ERhiFormat format) noexcept;
    [[nodiscard]] VkImageUsageFlags   ToVkImageUsage(ERhiTextureBindFlags flags) noexcept;
    [[nodiscard]] VkBufferUsageFlags  ToVkBufferUsage(ERhiBufferBindFlags flags) noexcept;
    [[nodiscard]] VkPrimitiveTopology ToVkPrimitiveTopology(ERhiPrimitiveTopology topo) noexcept;
    [[nodiscard]] VkCullModeFlags     ToVkCullMode(ERhiRasterCullMode mode) noexcept;
    [[nodiscard]] VkFrontFace         ToVkFrontFace(ERhiRasterFrontFace face) noexcept;
    [[nodiscard]] VkCompareOp         ToVkCompareOp(ERhiCompareOp op) noexcept;
    [[nodiscard]] VkBlendOp           ToVkBlendOp(ERhiBlendOp op) noexcept;
    [[nodiscard]] VkBlendFactor       ToVkBlendFactor(ERhiBlendFactor factor) noexcept;

    struct FStateMapping {
        VkPipelineStageFlags2 mStages = 0;
        VkAccessFlags2        mAccess = 0;
        VkImageLayout         mLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    };

    [[nodiscard]] FStateMapping MapResourceState(ERhiResourceState state, bool isDepth) noexcept;

    [[nodiscard]] inline bool   IsDepthFormat(ERhiFormat format) noexcept {
        return format == ERhiFormat::D24UnormS8Uint || format == ERhiFormat::D32Float;
    }
#endif
} // namespace AltinaEngine::Rhi::Vulkan::Detail
