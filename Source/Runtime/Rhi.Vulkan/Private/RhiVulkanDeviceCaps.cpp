#include "RhiVulkanDeviceCaps.h"

#include <cstring>

namespace AltinaEngine::Rhi::Vulkan::Detail {
#if defined(AE_RHI_VULKAN_AVAILABLE) && AE_RHI_VULKAN_AVAILABLE
    namespace {
        [[nodiscard]] auto HasDeviceExtension(
            const Core::Container::TVector<VkExtensionProperties>& props, const char* name) noexcept
            -> bool {
            if (name == nullptr) {
                return false;
            }
            for (const auto& p : props) {
                if (std::strcmp(p.extensionName, name) == 0) {
                    return true;
                }
            }
            return false;
        }

        void EnumerateDeviceExtensions(VkPhysicalDevice      physical,
            Core::Container::TVector<VkExtensionProperties>& out) noexcept {
            out.Clear();
            if (!physical) {
                return;
            }
            u32 count = 0U;
            if (vkEnumerateDeviceExtensionProperties(physical, nullptr, &count, nullptr)
                != VK_SUCCESS) {
                return;
            }
            if (count == 0U) {
                return;
            }
            out.Resize(count);
            if (vkEnumerateDeviceExtensionProperties(physical, nullptr, &count, out.Data())
                != VK_SUCCESS) {
                out.Clear();
                return;
            }
            out.Resize(count);
        }
    } // namespace

    void BuildDeviceCreateInfo(VkPhysicalDevice physical, FVulkanDeviceCreateInfo& out) noexcept {
        out = {};

        Core::Container::TVector<VkExtensionProperties> extProps;
        EnumerateDeviceExtensions(physical, extProps);

        // Query supported features.
        VkPhysicalDeviceFeatures2 features2{};
        features2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;

        VkPhysicalDeviceVulkan13Features features13{};
        features13.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
        VkPhysicalDeviceVulkan12Features features12{};
        features12.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
        VkPhysicalDeviceDescriptorIndexingFeatures descIndex{};
        descIndex.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_FEATURES;
        VkPhysicalDeviceTimelineSemaphoreFeatures timeline{};
        timeline.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_TIMELINE_SEMAPHORE_FEATURES;
        VkPhysicalDeviceSynchronization2Features sync2{};
        sync2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SYNCHRONIZATION_2_FEATURES;
        VkPhysicalDeviceDynamicRenderingFeatures dynRendering{};
        dynRendering.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_FEATURES;
        VkPhysicalDeviceExtendedDynamicStateFeaturesEXT extDyn{};
        extDyn.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTENDED_DYNAMIC_STATE_FEATURES_EXT;

        features2.pNext    = &features13;
        features13.pNext   = &features12;
        features12.pNext   = &descIndex;
        descIndex.pNext    = &timeline;
        timeline.pNext     = &sync2;
        sync2.pNext        = &dynRendering;
        dynRendering.pNext = &extDyn;

        vkGetPhysicalDeviceFeatures2(physical, &features2);

        const bool hasSync2Ext =
            HasDeviceExtension(extProps, VK_KHR_SYNCHRONIZATION_2_EXTENSION_NAME);
        const bool hasDynRenderingExt =
            HasDeviceExtension(extProps, VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME);
        const bool hasTimelineExt =
            HasDeviceExtension(extProps, VK_KHR_TIMELINE_SEMAPHORE_EXTENSION_NAME);
        const bool hasExtDynExt =
            HasDeviceExtension(extProps, VK_EXT_EXTENDED_DYNAMIC_STATE_EXTENSION_NAME);

        // Decide what to enable. Prefer enabling core feature structs even if the extension name
        // isn't present (when promoted to core). For the extension-only path, require both.
        out.mEnabled.mSync2 =
            (sync2.synchronization2 != 0) && (hasSync2Ext || features13.synchronization2);
        out.mEnabled.mDynamicRendering = (dynRendering.dynamicRendering != 0)
            && (hasDynRenderingExt || features13.dynamicRendering);
        out.mEnabled.mTimelineSemaphore =
            (timeline.timelineSemaphore != 0) && (hasTimelineExt || features12.timelineSemaphore);
        out.mEnabled.mExtendedDynamicState1 = (extDyn.extendedDynamicState != 0) && hasExtDynExt;

        // Mandatory for our swapchain-based viewport.
        out.mEnabledExtensions.PushBack(VK_KHR_SWAPCHAIN_EXTENSION_NAME);

        if (out.mEnabled.mSync2 && hasSync2Ext) {
            out.mEnabledExtensions.PushBack(VK_KHR_SYNCHRONIZATION_2_EXTENSION_NAME);
        }
        if (out.mEnabled.mDynamicRendering && hasDynRenderingExt) {
            out.mEnabledExtensions.PushBack(VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME);
        }
        if (out.mEnabled.mTimelineSemaphore && hasTimelineExt) {
            out.mEnabledExtensions.PushBack(VK_KHR_TIMELINE_SEMAPHORE_EXTENSION_NAME);
        }
        if (out.mEnabled.mExtendedDynamicState1) {
            out.mEnabledExtensions.PushBack(VK_EXT_EXTENDED_DYNAMIC_STATE_EXTENSION_NAME);
        }

        // Build enabled feature chain.
        out.mFeatures2.sType    = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
        out.mFeatures2.features = {};
        // Enable commonly used core features if supported.
        out.mFeatures2.features.samplerAnisotropy = features2.features.samplerAnisotropy;
        out.mFeatures2.features.fillModeNonSolid  = features2.features.fillModeNonSolid;

        out.mFeatures13.sType   = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
        out.mFeatures12.sType   = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
        out.mDescIndex.sType    = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_FEATURES;
        out.mTimeline.sType     = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_TIMELINE_SEMAPHORE_FEATURES;
        out.mSync2.sType        = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SYNCHRONIZATION_2_FEATURES;
        out.mDynRendering.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_FEATURES;
        out.mExtDyn.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTENDED_DYNAMIC_STATE_FEATURES_EXT;

        out.mTimeline.timelineSemaphore    = out.mEnabled.mTimelineSemaphore ? VK_TRUE : VK_FALSE;
        out.mSync2.synchronization2        = out.mEnabled.mSync2 ? VK_TRUE : VK_FALSE;
        out.mDynRendering.dynamicRendering = out.mEnabled.mDynamicRendering ? VK_TRUE : VK_FALSE;
        out.mExtDyn.extendedDynamicState = out.mEnabled.mExtendedDynamicState1 ? VK_TRUE : VK_FALSE;

        out.mFeatures2.pNext    = &out.mFeatures13;
        out.mFeatures13.pNext   = &out.mFeatures12;
        out.mFeatures12.pNext   = &out.mDescIndex;
        out.mDescIndex.pNext    = &out.mTimeline;
        out.mTimeline.pNext     = &out.mSync2;
        out.mSync2.pNext        = &out.mDynRendering;
        out.mDynRendering.pNext = &out.mExtDyn;
        out.mExtDyn.pNext       = nullptr;
    }
#else
    void BuildDeviceCreateInfo(VkPhysicalDevice, FVulkanDeviceCreateInfo& out) noexcept {
        out = {};
    }
#endif
} // namespace AltinaEngine::Rhi::Vulkan::Detail
