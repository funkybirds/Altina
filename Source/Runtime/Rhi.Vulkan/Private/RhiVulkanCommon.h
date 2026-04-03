#pragma once

#include <vulkan/vulkan.h>

namespace AltinaEngine::Rhi::SafeWrapper {
    template <typename T> auto GetInstanceProcAddrTs(VkInstance instance, const char* pName) -> T {
        return reinterpret_cast<T>(vkGetInstanceProcAddr(instance, pName)); // NOLINT
    }
} // namespace AltinaEngine::Rhi::SafeWrapper

namespace AltinaEngine::Rhi {
    auto VulkanResultAssert(VkResult result, FStringView message) -> bool {
        if (result != VK_SUCCESS) [[unlikely]] {
            LogErrorCat(
                TEXT("RHI.Vulkan"), TEXT("{} (VkResult: {})"), message, static_cast<i32>(result));
            return false;
        }
        return true;
    }
} // namespace AltinaEngine::Rhi