#pragma once

#include "RhiVulkanAPI.h"
#include "RhiVulkanInternal.h"
#include "Container/Vector.h"

namespace AltinaEngine::Rhi::Vulkan::Detail {
    namespace Container = Core::Container;

    struct FVulkanDeviceEnabledFeatures {
        bool mShaderDrawParameters  = false;
        bool mSync2                 = false;
        bool mDynamicRendering      = false;
        bool mTimelineSemaphore     = false;
        bool mExtendedDynamicState1 = false; // VK_EXT_extended_dynamic_state (primitive topology)
    };

    struct FVulkanDeviceCreateInfo {
        // Enabled feature chain that is safe to reference until vkCreateDevice returns.
        VkPhysicalDeviceFeatures2                       mFeatures2{};
        VkPhysicalDeviceVulkan13Features                mFeatures13{};
        VkPhysicalDeviceVulkan12Features                mFeatures12{};
        VkPhysicalDeviceVulkan11Features                mFeatures11{};
        VkPhysicalDeviceShaderDrawParametersFeatures    mShaderDrawParams{};
        VkPhysicalDeviceTimelineSemaphoreFeatures       mTimeline{};
        VkPhysicalDeviceSynchronization2Features        mSync2{};
        VkPhysicalDeviceDynamicRenderingFeatures        mDynRendering{};
        VkPhysicalDeviceExtendedDynamicStateFeaturesEXT mExtDyn{};

        Container::TVector<const char*>                 mEnabledExtensions;
        FVulkanDeviceEnabledFeatures                    mEnabled{};
    };

    // Builds the vkCreateDevice pNext feature chain and enabled device extension list.
    // Keep this logic centralized to avoid "queried but never enabled" feature mismatches.
    void BuildDeviceCreateInfo(VkPhysicalDevice physical, FVulkanDeviceCreateInfo& out) noexcept;
} // namespace AltinaEngine::Rhi::Vulkan::Detail
