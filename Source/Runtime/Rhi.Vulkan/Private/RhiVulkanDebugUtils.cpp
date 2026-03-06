#include "RhiVulkanDebugUtils.h"

#include "Logging/Log.h"
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

    void CmdBeginDebugLabel(
        VkDevice device, VkCommandBuffer commandBuffer, FStringView label) noexcept {
        static bool sLoggedInvalidInputs = false;
        static bool sLoggedMissingProc   = false;
        if (device == VK_NULL_HANDLE || commandBuffer == VK_NULL_HANDLE || label.IsEmpty()) {
            if (!sLoggedInvalidInputs) {
                sLoggedInvalidInputs = true;
                Core::Logging::LogWarningCat(TEXT("RHI.Vulkan.DebugMarker"),
                    TEXT(
                        "CmdBeginDebugLabel skipped: invalid inputs (device={}, cmd={}, emptyLabel={})."),
                    static_cast<const void*>(device), static_cast<const void*>(commandBuffer),
                    label.IsEmpty() ? 1 : 0);
            }
            return;
        }
        auto beginLabel = reinterpret_cast<PFN_vkCmdBeginDebugUtilsLabelEXT>(
            vkGetDeviceProcAddr(device, "vkCmdBeginDebugUtilsLabelEXT"));
        if (beginLabel == nullptr) {
            if (!sLoggedMissingProc) {
                sLoggedMissingProc = true;
                Core::Logging::LogWarningCat(TEXT("RHI.Vulkan.DebugMarker"),
                    TEXT(
                        "vkCmdBeginDebugUtilsLabelEXT unavailable (VK_EXT_debug_utils not enabled/supported)."));
            }
            return;
        }

        const FNativeString utf8Label = Core::Utility::String::ToUtf8Bytes(label);
        if (utf8Label.IsEmptyString()) {
            return;
        }

        VkDebugUtilsLabelEXT info{};
        info.sType      = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT;
        info.pLabelName = utf8Label.CStr();
        beginLabel(commandBuffer, &info);
    }

    void CmdEndDebugLabel(VkDevice device, VkCommandBuffer commandBuffer) noexcept {
        if (device == VK_NULL_HANDLE || commandBuffer == VK_NULL_HANDLE) {
            return;
        }
        auto endLabel = reinterpret_cast<PFN_vkCmdEndDebugUtilsLabelEXT>(
            vkGetDeviceProcAddr(device, "vkCmdEndDebugUtilsLabelEXT"));
        if (endLabel == nullptr) {
            return;
        }
        endLabel(commandBuffer);
    }

    void CmdInsertDebugLabel(
        VkDevice device, VkCommandBuffer commandBuffer, FStringView label) noexcept {
        if (device == VK_NULL_HANDLE || commandBuffer == VK_NULL_HANDLE || label.IsEmpty()) {
            return;
        }
        auto insertLabel = reinterpret_cast<PFN_vkCmdInsertDebugUtilsLabelEXT>(
            vkGetDeviceProcAddr(device, "vkCmdInsertDebugUtilsLabelEXT"));
        if (insertLabel == nullptr) {
            return;
        }

        const FNativeString utf8Label = Core::Utility::String::ToUtf8Bytes(label);
        if (utf8Label.IsEmptyString()) {
            return;
        }

        VkDebugUtilsLabelEXT info{};
        info.sType      = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT;
        info.pLabelName = utf8Label.CStr();
        insertLabel(commandBuffer, &info);
    }
} // namespace AltinaEngine::Rhi::Vulkan::Detail
