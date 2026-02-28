#pragma once

#if defined(AE_RHI_VULKAN_AVAILABLE) && AE_RHI_VULKAN_AVAILABLE
    #include <vulkan/vulkan.h>

namespace AltinaEngine::Rhi::SafeWrapper {
    template <typename T> auto GetInstanceProcAddrTs(VkInstance instance, const char* pName) -> T {
        return reinterpret_cast<T>(vkGetInstanceProcAddr(instance, pName)); // NOLINT
    }
} // namespace AltinaEngine::Rhi::SafeWrapper

#endif
