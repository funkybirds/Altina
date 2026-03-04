#pragma once

#include "RhiVulkanInternal.h"
#include "Container/String.h"
#include "Container/StringView.h"

namespace AltinaEngine::Rhi::Vulkan::Detail {
    namespace Container = Core::Container;
    using Container::FString;
    using Container::FStringView;

    template <typename THandle>
    [[nodiscard]] inline auto ToVkObjectHandle(THandle handle) noexcept -> u64 {
        if constexpr (requires { reinterpret_cast<u64>(handle); }) {
            return reinterpret_cast<u64>(handle);
        } else {
            return static_cast<u64>(handle);
        }
    }

    [[nodiscard]] inline auto BuildDebugObjectName(
        FStringView baseName, FStringView fallbackBaseName, FStringView suffix) -> FString {
        FString out;
        if (!baseName.IsEmpty()) {
            out.Append(baseName);
        } else {
            out.Append(fallbackBaseName);
        }
        if (!suffix.IsEmpty()) {
            out.Append(TEXT("."));
            out.Append(suffix);
        }
        return out;
    }

    void SetVkObjectDebugNameRaw(VkDevice device, u64 objectHandle, VkObjectType objectType,
        FStringView baseName, FStringView fallbackBaseName, FStringView suffix) noexcept;

    template <typename THandle>
    inline void SetVkObjectDebugName(VkDevice device, THandle handle, VkObjectType objectType,
        FStringView baseName, FStringView fallbackBaseName, FStringView suffix) noexcept {
        SetVkObjectDebugNameRaw(
            device, ToVkObjectHandle(handle), objectType, baseName, fallbackBaseName, suffix);
    }
} // namespace AltinaEngine::Rhi::Vulkan::Detail
