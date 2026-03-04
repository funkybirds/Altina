#include "TestHarness.h"

#include "RhiVulkan/RhiVulkanPipeline.h"

#include "Rhi/RhiStructs.h"

using AltinaEngine::u64;
using AltinaEngine::usize;
using AltinaEngine::Rhi::FRhiBindGroupDesc;
using AltinaEngine::Rhi::FRhiVulkanBindGroup;

namespace {
    auto MakeFakeDescriptorPool(u64 value) -> VkDescriptorPool {
#if defined(VK_USE_64_BIT_PTR_DEFINES) && VK_USE_64_BIT_PTR_DEFINES
        return reinterpret_cast<VkDescriptorPool>(static_cast<usize>(value));
#else
        return static_cast<VkDescriptorPool>(value);
#endif
    }

    auto MakeFakeDescriptorSet(u64 value) -> VkDescriptorSet {
#if defined(VK_USE_64_BIT_PTR_DEFINES) && VK_USE_64_BIT_PTR_DEFINES
        return reinterpret_cast<VkDescriptorSet>(static_cast<usize>(value));
#else
        return static_cast<VkDescriptorSet>(value);
#endif
    }
} // namespace

TEST_CASE("Rhi.Vulkan.BindGroup.ContextDescriptorMap") {
    FRhiBindGroupDesc      desc{};
    FRhiVulkanBindGroup    bindGroup(desc, VK_NULL_HANDLE, VK_NULL_HANDLE);

    void*                  contextA = reinterpret_cast<void*>(static_cast<usize>(1));
    void*                  contextB = reinterpret_cast<void*>(static_cast<usize>(2));

    const VkDescriptorPool poolA = MakeFakeDescriptorPool(10ULL);
    const VkDescriptorSet  setA  = MakeFakeDescriptorSet(100ULL);
    bindGroup.RegisterDescriptorSet(contextA, poolA, setA);
    REQUIRE(bindGroup.FindDescriptorSet(contextA) == setA);

    const VkDescriptorPool poolA2 = MakeFakeDescriptorPool(11ULL);
    const VkDescriptorSet  setA2  = MakeFakeDescriptorSet(101ULL);
    bindGroup.RegisterDescriptorSet(contextA, poolA2, setA2);
    REQUIRE(bindGroup.FindDescriptorSet(contextA) == setA2);

    const VkDescriptorPool poolB = MakeFakeDescriptorPool(12ULL);
    const VkDescriptorSet  setB  = MakeFakeDescriptorSet(102ULL);
    bindGroup.RegisterDescriptorSet(contextB, poolB, setB);
    REQUIRE(bindGroup.FindDescriptorSet(contextB) == setB);
    REQUIRE(bindGroup.FindDescriptorSet(contextA) == setA2);
}

TEST_CASE("Rhi.Vulkan.BindGroup.ContextInvalidation") {
    FRhiBindGroupDesc      desc{};
    FRhiVulkanBindGroup    bindGroup(desc, VK_NULL_HANDLE, VK_NULL_HANDLE);

    void*                  contextA = reinterpret_cast<void*>(static_cast<usize>(11));
    void*                  contextB = reinterpret_cast<void*>(static_cast<usize>(12));

    const VkDescriptorPool poolA = MakeFakeDescriptorPool(21ULL);
    const VkDescriptorSet  setA  = MakeFakeDescriptorSet(201ULL);
    const VkDescriptorPool poolB = MakeFakeDescriptorPool(22ULL);
    const VkDescriptorSet  setB  = MakeFakeDescriptorSet(202ULL);

    bindGroup.RegisterDescriptorSet(contextA, poolA, setA);
    bindGroup.RegisterDescriptorSet(contextB, poolB, setB);
    REQUIRE(bindGroup.FindDescriptorSet(contextA) == setA);
    REQUIRE(bindGroup.FindDescriptorSet(contextB) == setB);

    FRhiVulkanBindGroup::NotifyContextDestroyed(contextA);
    REQUIRE(bindGroup.FindDescriptorSet(contextA) == VK_NULL_HANDLE);
    REQUIRE(bindGroup.FindDescriptorSet(contextB) == setB);
}
