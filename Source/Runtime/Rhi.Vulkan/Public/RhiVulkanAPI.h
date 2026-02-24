#pragma once

#include "Base/AltinaBase.h"

#if defined(AE_RHI_VULKAN_BUILD)
    #define AE_RHI_VULKAN_API AE_DLLEXPORT
#else
    #define AE_RHI_VULKAN_API AE_DLLIMPORT
#endif

// Centralize Vulkan type availability so public headers compile consistently
// regardless of include order.
#if defined(AE_RHI_VULKAN_AVAILABLE) && AE_RHI_VULKAN_AVAILABLE
    #include <vulkan/vulkan.h>
#else
struct VkInstance_T;
struct VkPhysicalDevice_T;
struct VkDevice_T;
struct VkQueue_T;
struct VkSurfaceKHR_T;
struct VkSwapchainKHR_T;
struct VkSemaphore_T;
struct VkFence_T;
struct VkCommandBuffer_T;
struct VkCommandPool_T;
struct VkRenderPass_T;
struct VkPipeline_T;
struct VkPipelineLayout_T;
struct VkDescriptorSetLayout_T;
struct VkDescriptorSet_T;
struct VkBuffer_T;
struct VkImage_T;
struct VkImageView_T;
struct VkSampler_T;
struct VkShaderModule_T;

using VkInstance            = VkInstance_T*;
using VkPhysicalDevice      = VkPhysicalDevice_T*;
using VkDevice              = VkDevice_T*;
using VkQueue               = VkQueue_T*;
using VkSurfaceKHR          = VkSurfaceKHR_T*;
using VkSwapchainKHR        = VkSwapchainKHR_T*;
using VkSemaphore           = VkSemaphore_T*;
using VkFence               = VkFence_T*;
using VkCommandBuffer       = VkCommandBuffer_T*;
using VkCommandPool         = VkCommandPool_T*;
using VkRenderPass          = VkRenderPass_T*;
using VkPipeline            = VkPipeline_T*;
using VkPipelineLayout      = VkPipelineLayout_T*;
using VkDescriptorSetLayout = VkDescriptorSetLayout_T*;
using VkDescriptorSet       = VkDescriptorSet_T*;
using VkBuffer              = VkBuffer_T*;
using VkImage               = VkImage_T*;
using VkImageView           = VkImageView_T*;
using VkSampler             = VkSampler_T*;
using VkShaderModule        = VkShaderModule_T*;

struct VkPipelineRenderingCreateInfo;

enum VkPrimitiveTopology : int {
    VK_PRIMITIVE_TOPOLOGY_MAX_ENUM = 0x7FFFFFFF
};

    // Match Vulkan's handle convention: null handle is a nullptr for stub builds.
    #ifndef VK_NULL_HANDLE
        #define VK_NULL_HANDLE nullptr
    #endif
#endif
