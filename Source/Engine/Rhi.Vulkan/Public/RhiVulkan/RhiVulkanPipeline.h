#pragma once

#include "RhiVulkanAPI.h"
#include "Rhi/RhiPipeline.h"
#include "Rhi/RhiPipelineLayout.h"
#include "Rhi/RhiBindGroup.h"
#include "Rhi/RhiBindGroupLayout.h"
#include "Rhi/RhiRefs.h"

#if defined(AE_RHI_VULKAN_AVAILABLE) && AE_RHI_VULKAN_AVAILABLE
    #include <vulkan/vulkan.h>
#else
struct VkPipeline_T;
struct VkPipelineLayout_T;
struct VkDescriptorSetLayout_T;
struct VkDescriptorSet_T;
using VkPipeline            = VkPipeline_T*;
using VkPipelineLayout      = VkPipelineLayout_T*;
using VkDescriptorSetLayout = VkDescriptorSetLayout_T*;
using VkDescriptorSet       = VkDescriptorSet_T*;
#endif

namespace AltinaEngine::Rhi {
    class AE_RHI_VULKAN_API FRhiVulkanPipelineLayout final : public FRhiPipelineLayout {
    public:
        FRhiVulkanPipelineLayout(const FRhiPipelineLayoutDesc& desc, VkDevice device);
        FRhiVulkanPipelineLayout(const FRhiPipelineLayoutDesc& desc, VkDevice device,
            VkPipelineLayout layout, bool ownsLayout);
        ~FRhiVulkanPipelineLayout() override;

        [[nodiscard]] auto GetNativeLayout() const noexcept -> VkPipelineLayout;

    private:
        struct FState;
        FState* mState = nullptr;
    };

    class AE_RHI_VULKAN_API FRhiVulkanBindGroupLayout final : public FRhiBindGroupLayout {
    public:
        FRhiVulkanBindGroupLayout(const FRhiBindGroupLayoutDesc& desc, VkDevice device);
        FRhiVulkanBindGroupLayout(const FRhiBindGroupLayoutDesc& desc, VkDevice device,
            VkDescriptorSetLayout layout, bool ownsLayout);
        ~FRhiVulkanBindGroupLayout() override;

        [[nodiscard]] auto GetNativeLayout() const noexcept -> VkDescriptorSetLayout;

    private:
        struct FState;
        FState* mState = nullptr;
    };

    class AE_RHI_VULKAN_API FRhiVulkanBindGroup final : public FRhiBindGroup {
    public:
        FRhiVulkanBindGroup(const FRhiBindGroupDesc& desc, VkDevice device, VkDescriptorSet set);
        ~FRhiVulkanBindGroup() override;

        [[nodiscard]] auto GetDescriptorSet() const noexcept -> VkDescriptorSet;

    private:
        struct FState;
        FState* mState = nullptr;
    };

    class AE_RHI_VULKAN_API FRhiVulkanGraphicsPipeline final : public FRhiPipeline {
    public:
        FRhiVulkanGraphicsPipeline(const FRhiGraphicsPipelineDesc& desc, VkDevice device);
        ~FRhiVulkanGraphicsPipeline() override;

        [[nodiscard]] auto GetNativePipeline() const noexcept -> VkPipeline;
        [[nodiscard]] auto GetNativeLayout() const noexcept -> VkPipelineLayout;

    private:
        [[nodiscard]] auto GetOrCreatePipeline(u64 attachmentHash, VkRenderPass renderPass,
            const VkPipelineRenderingCreateInfo* renderingInfo, VkPrimitiveTopology topology)
            -> VkPipeline;

        struct FState;
        FState*               mState = nullptr;
        FRhiPipelineLayoutRef mPipelineLayout;
        FRhiShaderRef         mVertexShader;
        FRhiShaderRef         mPixelShader;
        FRhiShaderRef         mGeometryShader;
        FRhiShaderRef         mHullShader;
        FRhiShaderRef         mDomainShader;

        friend class FRhiVulkanCommandContext;
    };

    class AE_RHI_VULKAN_API FRhiVulkanComputePipeline final : public FRhiPipeline {
    public:
        FRhiVulkanComputePipeline(const FRhiComputePipelineDesc& desc, VkDevice device);
        ~FRhiVulkanComputePipeline() override;

        [[nodiscard]] auto GetNativePipeline() const noexcept -> VkPipeline;
        [[nodiscard]] auto GetNativeLayout() const noexcept -> VkPipelineLayout;

    private:
        struct FState;
        FState*               mState = nullptr;
        FRhiPipelineLayoutRef mPipelineLayout;
        FRhiShaderRef         mComputeShader;
    };

} // namespace AltinaEngine::Rhi
