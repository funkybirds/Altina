#include "RhiVulkanDebugUtils.h"

#include "Utility/String/CodeConvert.h"

namespace AltinaEngine::Rhi::Vulkan::Detail {
    using Core::Container::FNativeString;

    void SetVkObjectDebugNameRaw(VkDevice device, u64 objectHandle, VkObjectType objectType,
        FStringView baseName, FStringView fallbackBaseName, FStringView suffix) noexcept {
        if (device == VK_NULL_HANDLE || objectHandle == 0ULL) {
            return;
        }

        const FString       objectName = BuildDebugObjectName(baseName, fallbackBaseName, suffix);
        const FNativeString utf8Name   = Core::Utility::String::ToUtf8Bytes(objectName);
        if (utf8Name.IsEmptyString()) {
            return;
        }

        auto setObjectName = reinterpret_cast<PFN_vkSetDebugUtilsObjectNameEXT>(
            vkGetDeviceProcAddr(device, "vkSetDebugUtilsObjectNameEXT"));
        if (setObjectName == nullptr) {
            return;
        }

        VkDebugUtilsObjectNameInfoEXT nameInfo{};
        nameInfo.sType        = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT;
        nameInfo.objectType   = objectType;
        nameInfo.objectHandle = objectHandle;
        nameInfo.pObjectName  = utf8Name.CStr();
        (void)setObjectName(device, &nameInfo);
    }
} // namespace AltinaEngine::Rhi::Vulkan::Detail
