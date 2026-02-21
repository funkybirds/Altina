#pragma once

#include "RhiVulkanAPI.h"

#ifdef CreateSemaphore
    #undef CreateSemaphore
#endif
#ifdef CreateSemaphoreA
    #undef CreateSemaphoreA
#endif
#ifdef CreateSemaphoreW
    #undef CreateSemaphoreW
#endif

#include "Rhi/RhiDevice.h"

// Some Windows headers included by dependencies may reintroduce these macros.
#ifdef CreateSemaphore
    #undef CreateSemaphore
#endif
#ifdef CreateSemaphoreA
    #undef CreateSemaphoreA
#endif
#ifdef CreateSemaphoreW
    #undef CreateSemaphoreW
#endif

#if defined(AE_RHI_VULKAN_AVAILABLE) && AE_RHI_VULKAN_AVAILABLE
    #include <vulkan/vulkan.h>
#else
struct VkInstance_T;
struct VkPhysicalDevice_T;
struct VkDevice_T;
struct VkQueue_T;
using VkInstance       = VkInstance_T*;
using VkPhysicalDevice = VkPhysicalDevice_T*;
using VkDevice         = VkDevice_T*;
using VkQueue          = VkQueue_T*;
#endif

namespace AltinaEngine::Rhi {
    class AE_RHI_VULKAN_API FRhiVulkanDevice final : public FRhiDevice {
    public:
        FRhiVulkanDevice(const FRhiDeviceDesc& desc, const FRhiAdapterDesc& adapterDesc,
            VkInstance instance, VkPhysicalDevice physicalDevice, VkDevice device);
        ~FRhiVulkanDevice() override;

        FRhiVulkanDevice(const FRhiVulkanDevice&)                                  = delete;
        FRhiVulkanDevice(FRhiVulkanDevice&&)                                       = delete;
        auto               operator=(const FRhiVulkanDevice&) -> FRhiVulkanDevice& = delete;
        auto               operator=(FRhiVulkanDevice&&) -> FRhiVulkanDevice&      = delete;

        [[nodiscard]] auto GetNativeInstance() const noexcept -> VkInstance;
        [[nodiscard]] auto GetPhysicalDevice() const noexcept -> VkPhysicalDevice;
        [[nodiscard]] auto GetNativeDevice() const noexcept -> VkDevice;
        [[nodiscard]] auto GetQueueFamilyIndex(ERhiQueueType type) const noexcept -> u32;
        [[nodiscard]] auto SupportsDynamicRendering() const noexcept -> bool;
        [[nodiscard]] auto SupportsSynchronization2() const noexcept -> bool;
        [[nodiscard]] auto SupportsExtendedDynamicState() const noexcept -> bool;
        [[nodiscard]] auto GetInternalAllocatorHandle() const noexcept -> void*;

        void               NotifyViewportAcquired(VkSemaphore acquire, VkSemaphore renderComplete);
        [[nodiscard]] auto ConsumePendingAcquireSemaphore() noexcept -> VkSemaphore;
        [[nodiscard]] auto ConsumePendingRenderCompleteSemaphore() noexcept -> VkSemaphore;

        auto               CreateBuffer(const FRhiBufferDesc& desc) -> FRhiBufferRef override;
        auto               CreateTexture(const FRhiTextureDesc& desc) -> FRhiTextureRef override;
        auto               CreateShaderResourceView(const FRhiShaderResourceViewDesc& desc)
            -> FRhiShaderResourceViewRef override;
        auto CreateUnorderedAccessView(const FRhiUnorderedAccessViewDesc& desc)
            -> FRhiUnorderedAccessViewRef override;
        auto CreateRenderTargetView(const FRhiRenderTargetViewDesc& desc)
            -> FRhiRenderTargetViewRef override;
        auto CreateDepthStencilView(const FRhiDepthStencilViewDesc& desc)
            -> FRhiDepthStencilViewRef override;
        auto CreateViewport(const FRhiViewportDesc& desc) -> FRhiViewportRef override;
        auto CreateSampler(const FRhiSamplerDesc& desc) -> FRhiSamplerRef override;
        auto CreateShader(const FRhiShaderDesc& desc) -> FRhiShaderRef override;

        auto CreateGraphicsPipeline(const FRhiGraphicsPipelineDesc& desc)
            -> FRhiPipelineRef override;
        auto CreateComputePipeline(const FRhiComputePipelineDesc& desc) -> FRhiPipelineRef override;
        auto CreatePipelineLayout(const FRhiPipelineLayoutDesc& desc)
            -> FRhiPipelineLayoutRef override;

        auto CreateBindGroupLayout(const FRhiBindGroupLayoutDesc& desc)
            -> FRhiBindGroupLayoutRef override;
        auto CreateBindGroup(const FRhiBindGroupDesc& desc) -> FRhiBindGroupRef override;

        auto CreateFence(u64 initialValue) -> FRhiFenceRef override;
#ifdef CreateSemaphore
    #undef CreateSemaphore
#endif
#ifdef CreateSemaphoreA
    #undef CreateSemaphoreA
#endif
#ifdef CreateSemaphoreW
    #undef CreateSemaphoreW
#endif
        auto CreateSemaphore(bool timeline, u64 initialValue) -> FRhiSemaphoreRef override;

        auto CreateCommandPool(const FRhiCommandPoolDesc& desc) -> FRhiCommandPoolRef override;
        auto CreateCommandList(const FRhiCommandListDesc& desc) -> FRhiCommandListRef override;
        auto CreateCommandContext(const FRhiCommandContextDesc& desc)
            -> FRhiCommandContextRef override;

        void BeginFrame(u64 frameIndex) override;
        void EndFrame() override;

    private:
        struct FState;
        FState* mState = nullptr;
    };

} // namespace AltinaEngine::Rhi
