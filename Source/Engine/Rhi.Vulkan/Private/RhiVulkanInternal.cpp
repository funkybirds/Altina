#include "RhiVulkanInternal.h"

namespace AltinaEngine::Rhi::Vulkan::Detail {
#if defined(AE_RHI_VULKAN_AVAILABLE) && AE_RHI_VULKAN_AVAILABLE
    VkFormat ToVkFormat(ERhiFormat format) noexcept {
        switch (format) {
            case ERhiFormat::R8G8B8A8Unorm:
                return VK_FORMAT_R8G8B8A8_UNORM;
            case ERhiFormat::R8G8B8A8UnormSrgb:
                return VK_FORMAT_R8G8B8A8_SRGB;
            case ERhiFormat::B8G8R8A8Unorm:
                return VK_FORMAT_B8G8R8A8_UNORM;
            case ERhiFormat::B8G8R8A8UnormSrgb:
                return VK_FORMAT_B8G8R8A8_SRGB;
            case ERhiFormat::R16G16B16A16Float:
                return VK_FORMAT_R16G16B16A16_SFLOAT;
            case ERhiFormat::R32Float:
                return VK_FORMAT_R32_SFLOAT;
            case ERhiFormat::D24UnormS8Uint:
                return VK_FORMAT_D24_UNORM_S8_UINT;
            case ERhiFormat::D32Float:
                return VK_FORMAT_D32_SFLOAT;
            default:
                return VK_FORMAT_UNDEFINED;
        }
    }

    VkImageAspectFlags ToVkAspectFlags(ERhiFormat format) noexcept {
        switch (format) {
            case ERhiFormat::D24UnormS8Uint:
                return VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
            case ERhiFormat::D32Float:
                return VK_IMAGE_ASPECT_DEPTH_BIT;
            default:
                return VK_IMAGE_ASPECT_COLOR_BIT;
        }
    }

    VkImageUsageFlags ToVkImageUsage(ERhiTextureBindFlags flags) noexcept {
        VkImageUsageFlags usage = 0;
        if (HasAnyFlags(flags, ERhiTextureBindFlags::ShaderResource)) {
            usage |= VK_IMAGE_USAGE_SAMPLED_BIT;
        }
        if (HasAnyFlags(flags, ERhiTextureBindFlags::RenderTarget)) {
            usage |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
        }
        if (HasAnyFlags(flags, ERhiTextureBindFlags::DepthStencil)) {
            usage |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
        }
        if (HasAnyFlags(flags, ERhiTextureBindFlags::UnorderedAccess)) {
            usage |= VK_IMAGE_USAGE_STORAGE_BIT;
        }
        if (HasAnyFlags(flags, ERhiTextureBindFlags::CopySrc)) {
            usage |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
        }
        if (HasAnyFlags(flags, ERhiTextureBindFlags::CopyDst)) {
            usage |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
        }
        return usage;
    }

    VkBufferUsageFlags ToVkBufferUsage(ERhiBufferBindFlags flags) noexcept {
        VkBufferUsageFlags usage = 0;
        if (HasAnyFlags(flags, ERhiBufferBindFlags::Vertex)) {
            usage |= VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
        }
        if (HasAnyFlags(flags, ERhiBufferBindFlags::Index)) {
            usage |= VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
        }
        if (HasAnyFlags(flags, ERhiBufferBindFlags::Constant)) {
            usage |= VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
        }
        if (HasAnyFlags(flags, ERhiBufferBindFlags::ShaderResource)) {
            usage |= VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
        }
        if (HasAnyFlags(flags, ERhiBufferBindFlags::UnorderedAccess)) {
            usage |= VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
        }
        if (HasAnyFlags(flags, ERhiBufferBindFlags::Indirect)) {
            usage |= VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT;
        }
        if (HasAnyFlags(flags, ERhiBufferBindFlags::CopySrc)) {
            usage |= VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
        }
        if (HasAnyFlags(flags, ERhiBufferBindFlags::CopyDst)) {
            usage |= VK_BUFFER_USAGE_TRANSFER_DST_BIT;
        }
        return usage;
    }

    VkPrimitiveTopology ToVkPrimitiveTopology(ERhiPrimitiveTopology topo) noexcept {
        switch (topo) {
            case ERhiPrimitiveTopology::PointList:
                return VK_PRIMITIVE_TOPOLOGY_POINT_LIST;
            case ERhiPrimitiveTopology::LineList:
                return VK_PRIMITIVE_TOPOLOGY_LINE_LIST;
            case ERhiPrimitiveTopology::LineStrip:
                return VK_PRIMITIVE_TOPOLOGY_LINE_STRIP;
            case ERhiPrimitiveTopology::TriangleStrip:
                return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
            case ERhiPrimitiveTopology::TriangleList:
            default:
                return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        }
    }

    VkCullModeFlags ToVkCullMode(ERhiRasterCullMode mode) noexcept {
        switch (mode) {
            case ERhiRasterCullMode::Front:
                return VK_CULL_MODE_FRONT_BIT;
            case ERhiRasterCullMode::Back:
                return VK_CULL_MODE_BACK_BIT;
            case ERhiRasterCullMode::None:
            default:
                return VK_CULL_MODE_NONE;
        }
    }

    VkFrontFace ToVkFrontFace(ERhiRasterFrontFace face) noexcept {
        return (face == ERhiRasterFrontFace::CW) ? VK_FRONT_FACE_CLOCKWISE
                                                 : VK_FRONT_FACE_COUNTER_CLOCKWISE;
    }

    VkCompareOp ToVkCompareOp(ERhiCompareOp op) noexcept {
        switch (op) {
            case ERhiCompareOp::Never:
                return VK_COMPARE_OP_NEVER;
            case ERhiCompareOp::Less:
                return VK_COMPARE_OP_LESS;
            case ERhiCompareOp::Equal:
                return VK_COMPARE_OP_EQUAL;
            case ERhiCompareOp::LessEqual:
                return VK_COMPARE_OP_LESS_OR_EQUAL;
            case ERhiCompareOp::Greater:
                return VK_COMPARE_OP_GREATER;
            case ERhiCompareOp::NotEqual:
                return VK_COMPARE_OP_NOT_EQUAL;
            case ERhiCompareOp::GreaterEqual:
                return VK_COMPARE_OP_GREATER_OR_EQUAL;
            case ERhiCompareOp::Always:
            default:
                return VK_COMPARE_OP_ALWAYS;
        }
    }

    VkBlendOp ToVkBlendOp(ERhiBlendOp op) noexcept {
        switch (op) {
            case ERhiBlendOp::Subtract:
                return VK_BLEND_OP_SUBTRACT;
            case ERhiBlendOp::ReverseSubtract:
                return VK_BLEND_OP_REVERSE_SUBTRACT;
            case ERhiBlendOp::Min:
                return VK_BLEND_OP_MIN;
            case ERhiBlendOp::Max:
                return VK_BLEND_OP_MAX;
            case ERhiBlendOp::Add:
            default:
                return VK_BLEND_OP_ADD;
        }
    }

    VkBlendFactor ToVkBlendFactor(ERhiBlendFactor factor) noexcept {
        switch (factor) {
            case ERhiBlendFactor::Zero:
                return VK_BLEND_FACTOR_ZERO;
            case ERhiBlendFactor::One:
                return VK_BLEND_FACTOR_ONE;
            case ERhiBlendFactor::SrcColor:
                return VK_BLEND_FACTOR_SRC_COLOR;
            case ERhiBlendFactor::InvSrcColor:
                return VK_BLEND_FACTOR_ONE_MINUS_SRC_COLOR;
            case ERhiBlendFactor::SrcAlpha:
                return VK_BLEND_FACTOR_SRC_ALPHA;
            case ERhiBlendFactor::InvSrcAlpha:
                return VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
            case ERhiBlendFactor::DestAlpha:
                return VK_BLEND_FACTOR_DST_ALPHA;
            case ERhiBlendFactor::InvDestAlpha:
                return VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA;
            case ERhiBlendFactor::DestColor:
                return VK_BLEND_FACTOR_DST_COLOR;
            case ERhiBlendFactor::InvDestColor:
                return VK_BLEND_FACTOR_ONE_MINUS_DST_COLOR;
            case ERhiBlendFactor::SrcAlphaSaturate:
                return VK_BLEND_FACTOR_SRC_ALPHA_SATURATE;
            case ERhiBlendFactor::ConstantColor:
                return VK_BLEND_FACTOR_CONSTANT_COLOR;
            case ERhiBlendFactor::InvConstantColor:
                return VK_BLEND_FACTOR_ONE_MINUS_CONSTANT_COLOR;
            case ERhiBlendFactor::ConstantAlpha:
                return VK_BLEND_FACTOR_CONSTANT_ALPHA;
            case ERhiBlendFactor::InvConstantAlpha:
                return VK_BLEND_FACTOR_ONE_MINUS_CONSTANT_ALPHA;
            default:
                return VK_BLEND_FACTOR_ONE;
        }
    }

    FStateMapping MapResourceState(ERhiResourceState state, bool isDepth) noexcept {
        FStateMapping mapping{};
        switch (state) {
            case ERhiResourceState::RenderTarget:
                mapping.mStages = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
                mapping.mAccess = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
                mapping.mLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
                break;
            case ERhiResourceState::DepthWrite:
                mapping.mStages = VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT
                    | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT;
                mapping.mAccess = VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
                mapping.mLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
                break;
            case ERhiResourceState::DepthRead:
                mapping.mStages = VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT
                    | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT;
                mapping.mAccess = VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT;
                mapping.mLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
                break;
            case ERhiResourceState::ShaderResource:
                mapping.mStages = VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT
                    | VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
                mapping.mAccess = VK_ACCESS_2_SHADER_READ_BIT;
                mapping.mLayout = isDepth ? VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL
                                          : VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
                break;
            case ERhiResourceState::UnorderedAccess:
                mapping.mStages = VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT
                    | VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
                mapping.mAccess = VK_ACCESS_2_SHADER_READ_BIT | VK_ACCESS_2_SHADER_WRITE_BIT;
                mapping.mLayout = VK_IMAGE_LAYOUT_GENERAL;
                break;
            case ERhiResourceState::CopySrc:
                mapping.mStages = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
                mapping.mAccess = VK_ACCESS_2_TRANSFER_READ_BIT;
                mapping.mLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
                break;
            case ERhiResourceState::CopyDst:
                mapping.mStages = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
                mapping.mAccess = VK_ACCESS_2_TRANSFER_WRITE_BIT;
                mapping.mLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
                break;
            case ERhiResourceState::Present:
                mapping.mStages = VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT;
                mapping.mAccess = 0;
                mapping.mLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
                break;
            case ERhiResourceState::Common:
                mapping.mStages = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
                mapping.mAccess = 0;
                mapping.mLayout = VK_IMAGE_LAYOUT_GENERAL;
                break;
            case ERhiResourceState::Unknown:
            default:
                mapping.mStages = VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT;
                mapping.mAccess = 0;
                mapping.mLayout = VK_IMAGE_LAYOUT_UNDEFINED;
                break;
        }
        return mapping;
    }
#endif
} // namespace AltinaEngine::Rhi::Vulkan::Detail
