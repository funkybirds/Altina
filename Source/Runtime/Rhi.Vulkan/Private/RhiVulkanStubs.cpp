#include "RhiVulkan/RhiVulkanCommandContext.h"
#include "RhiVulkan/RhiVulkanDevice.h"
#include "RhiVulkan/RhiVulkanPipeline.h"
#include "RhiVulkan/RhiVulkanResources.h"
#include "RhiVulkan/RhiVulkanViewport.h"
#include "Rhi/RhiCommandList.h"
#include "Rhi/RhiCommandPool.h"
#include "Rhi/RhiFence.h"
#include "Rhi/RhiSemaphore.h"

namespace AltinaEngine::Rhi {
    struct FRhiVulkanDevice::FState {};
    struct FRhiVulkanViewport::FState {};
    struct FRhiVulkanPipelineLayout::FState {};
    struct FRhiVulkanBindGroupLayout::FState {};
    struct FRhiVulkanBindGroup::FState {};
    struct FRhiVulkanGraphicsPipeline::FState {};
    struct FRhiVulkanComputePipeline::FState {};
    struct FRhiVulkanBuffer::FState {};
    struct FRhiVulkanTexture::FState {};
    struct FRhiVulkanSampler::FState {};
    struct FRhiVulkanShader::FState {};
    struct FRhiVulkanShaderResourceView::FState {};
    struct FRhiVulkanUnorderedAccessView::FState {};
    struct FRhiVulkanRenderTargetView::FState {};
    struct FRhiVulkanDepthStencilView::FState {};

    FRhiVulkanDevice::FRhiVulkanDevice(const FRhiDeviceDesc& desc,
        const FRhiAdapterDesc& adapterDesc, VkInstance instance, VkPhysicalDevice physicalDevice,
        VkDevice device)
        : FRhiDevice(desc, adapterDesc) {
        (void)instance;
        (void)physicalDevice;
        (void)device;
    }

    FRhiVulkanDevice::~FRhiVulkanDevice() = default;

    auto FRhiVulkanDevice::GetNativeInstance() const noexcept -> VkInstance {
        return VK_NULL_HANDLE;
    }
    auto FRhiVulkanDevice::GetPhysicalDevice() const noexcept -> VkPhysicalDevice {
        return VK_NULL_HANDLE;
    }
    auto FRhiVulkanDevice::GetNativeDevice() const noexcept -> VkDevice { return VK_NULL_HANDLE; }
    auto FRhiVulkanDevice::GetQueueFamilyIndex(ERhiQueueType type) const noexcept -> u32 {
        (void)type;
        return 0U;
    }
    auto FRhiVulkanDevice::SupportsDynamicRendering() const noexcept -> bool { return false; }
    auto FRhiVulkanDevice::SupportsSynchronization2() const noexcept -> bool { return false; }
    auto FRhiVulkanDevice::SupportsExtendedDynamicState() const noexcept -> bool { return false; }
    auto FRhiVulkanDevice::GetInternalAllocatorHandle() const noexcept -> void* { return nullptr; }

    void FRhiVulkanDevice::NotifyViewportAcquired(VkSemaphore acquire, VkSemaphore renderComplete) {
        (void)acquire;
        (void)renderComplete;
    }
    auto FRhiVulkanDevice::ConsumePendingAcquireSemaphore() noexcept -> VkSemaphore {
        return VK_NULL_HANDLE;
    }
    auto FRhiVulkanDevice::ConsumePendingRenderCompleteSemaphore() noexcept -> VkSemaphore {
        return VK_NULL_HANDLE;
    }

    auto FRhiVulkanDevice::CreateBuffer(const FRhiBufferDesc& desc) -> FRhiBufferRef {
        (void)desc;
        return {};
    }
    auto FRhiVulkanDevice::CreateTexture(const FRhiTextureDesc& desc) -> FRhiTextureRef {
        (void)desc;
        return {};
    }
    auto FRhiVulkanDevice::CreateShaderResourceView(const FRhiShaderResourceViewDesc& desc)
        -> FRhiShaderResourceViewRef {
        (void)desc;
        return {};
    }
    auto FRhiVulkanDevice::CreateUnorderedAccessView(const FRhiUnorderedAccessViewDesc& desc)
        -> FRhiUnorderedAccessViewRef {
        (void)desc;
        return {};
    }
    auto FRhiVulkanDevice::CreateRenderTargetView(const FRhiRenderTargetViewDesc& desc)
        -> FRhiRenderTargetViewRef {
        (void)desc;
        return {};
    }
    auto FRhiVulkanDevice::CreateDepthStencilView(const FRhiDepthStencilViewDesc& desc)
        -> FRhiDepthStencilViewRef {
        (void)desc;
        return {};
    }
    auto FRhiVulkanDevice::CreateViewport(const FRhiViewportDesc& desc) -> FRhiViewportRef {
        (void)desc;
        return {};
    }
    auto FRhiVulkanDevice::CreateSampler(const FRhiSamplerDesc& desc) -> FRhiSamplerRef {
        (void)desc;
        return {};
    }
    auto FRhiVulkanDevice::CreateShader(const FRhiShaderDesc& desc) -> FRhiShaderRef {
        (void)desc;
        return {};
    }

    auto FRhiVulkanDevice::CreateGraphicsPipeline(const FRhiGraphicsPipelineDesc& desc)
        -> FRhiPipelineRef {
        (void)desc;
        return {};
    }
    auto FRhiVulkanDevice::CreateComputePipeline(const FRhiComputePipelineDesc& desc)
        -> FRhiPipelineRef {
        (void)desc;
        return {};
    }
    auto FRhiVulkanDevice::CreatePipelineLayout(const FRhiPipelineLayoutDesc& desc)
        -> FRhiPipelineLayoutRef {
        (void)desc;
        return {};
    }

    auto FRhiVulkanDevice::CreateBindGroupLayout(const FRhiBindGroupLayoutDesc& desc)
        -> FRhiBindGroupLayoutRef {
        (void)desc;
        return {};
    }
    auto FRhiVulkanDevice::CreateBindGroup(const FRhiBindGroupDesc& desc) -> FRhiBindGroupRef {
        (void)desc;
        return {};
    }

    auto FRhiVulkanDevice::CreateFence(u64 initialValue) -> FRhiFenceRef {
        (void)initialValue;
        return {};
    }
    auto FRhiVulkanDevice::CreateSemaphore(bool timeline, u64 initialValue) -> FRhiSemaphoreRef {
        (void)timeline;
        (void)initialValue;
        return {};
    }

    auto FRhiVulkanDevice::CreateCommandPool(const FRhiCommandPoolDesc& desc)
        -> FRhiCommandPoolRef {
        (void)desc;
        return {};
    }
    auto FRhiVulkanDevice::CreateCommandList(const FRhiCommandListDesc& desc)
        -> FRhiCommandListRef {
        (void)desc;
        return {};
    }
    auto FRhiVulkanDevice::CreateCommandContext(const FRhiCommandContextDesc& desc)
        -> FRhiCommandContextRef {
        (void)desc;
        return {};
    }

    void FRhiVulkanDevice::BeginFrame(u64 frameIndex) { (void)frameIndex; }
    void FRhiVulkanDevice::EndFrame() {}

    FRhiVulkanViewport::FRhiVulkanViewport(const FRhiViewportDesc& desc, VkInstance instance,
        VkDevice device, VkPhysicalDevice physicalDevice, VkQueue graphicsQueue,
        u32 graphicsQueueFamily)
        : FRhiViewport(desc) {
        (void)instance;
        (void)device;
        (void)physicalDevice;
        (void)graphicsQueue;
        (void)graphicsQueueFamily;
    }

    FRhiVulkanViewport::~FRhiVulkanViewport() = default;

    void FRhiVulkanViewport::Resize(u32 width, u32 height) {
        (void)width;
        (void)height;
    }
    auto FRhiVulkanViewport::GetBackBuffer() const noexcept -> FRhiTexture* { return nullptr; }
    void FRhiVulkanViewport::Present(const FRhiPresentInfo& info) { (void)info; }

    auto FRhiVulkanViewport::GetNativeSwapchain() const noexcept -> VkSwapchainKHR {
        return VK_NULL_HANDLE;
    }
    auto FRhiVulkanViewport::GetAcquireSemaphore() const noexcept -> VkSemaphore {
        return VK_NULL_HANDLE;
    }
    auto FRhiVulkanViewport::GetRenderCompleteSemaphore() const noexcept -> VkSemaphore {
        return VK_NULL_HANDLE;
    }
    auto FRhiVulkanViewport::GetCurrentImageIndex() const noexcept -> u32 { return 0U; }

    FRhiVulkanPipelineLayout::FRhiVulkanPipelineLayout(
        const FRhiPipelineLayoutDesc& desc, VkDevice device)
        : FRhiPipelineLayout(desc) {
        (void)device;
    }
    FRhiVulkanPipelineLayout::FRhiVulkanPipelineLayout(const FRhiPipelineLayoutDesc& desc,
        VkDevice device, VkPipelineLayout layout, bool ownsLayout)
        : FRhiPipelineLayout(desc) {
        (void)device;
        (void)layout;
        (void)ownsLayout;
    }
    FRhiVulkanPipelineLayout::~FRhiVulkanPipelineLayout() = default;
    auto FRhiVulkanPipelineLayout::GetNativeLayout() const noexcept -> VkPipelineLayout {
        return VK_NULL_HANDLE;
    }

    FRhiVulkanBindGroupLayout::FRhiVulkanBindGroupLayout(
        const FRhiBindGroupLayoutDesc& desc, VkDevice device)
        : FRhiBindGroupLayout(desc) {
        (void)device;
    }
    FRhiVulkanBindGroupLayout::FRhiVulkanBindGroupLayout(const FRhiBindGroupLayoutDesc& desc,
        VkDevice device, VkDescriptorSetLayout layout, bool ownsLayout)
        : FRhiBindGroupLayout(desc) {
        (void)device;
        (void)layout;
        (void)ownsLayout;
    }
    FRhiVulkanBindGroupLayout::~FRhiVulkanBindGroupLayout() = default;
    auto FRhiVulkanBindGroupLayout::GetNativeLayout() const noexcept -> VkDescriptorSetLayout {
        return VK_NULL_HANDLE;
    }

    FRhiVulkanBindGroup::FRhiVulkanBindGroup(
        const FRhiBindGroupDesc& desc, VkDevice device, VkDescriptorSet set)
        : FRhiBindGroup(desc) {
        (void)device;
        (void)set;
    }
    FRhiVulkanBindGroup::~FRhiVulkanBindGroup() = default;
    auto FRhiVulkanBindGroup::GetDescriptorSet() const noexcept -> VkDescriptorSet {
        return VK_NULL_HANDLE;
    }

    FRhiVulkanGraphicsPipeline::FRhiVulkanGraphicsPipeline(
        const FRhiGraphicsPipelineDesc& desc, VkDevice device)
        : FRhiPipeline(desc) {
        (void)device;
    }
    FRhiVulkanGraphicsPipeline::~FRhiVulkanGraphicsPipeline() = default;
    auto FRhiVulkanGraphicsPipeline::GetNativePipeline() const noexcept -> VkPipeline {
        return VK_NULL_HANDLE;
    }
    auto FRhiVulkanGraphicsPipeline::GetNativeLayout() const noexcept -> VkPipelineLayout {
        return VK_NULL_HANDLE;
    }
    auto FRhiVulkanGraphicsPipeline::GetOrCreatePipeline(u64 attachmentHash,
        VkRenderPass renderPass, const VkPipelineRenderingCreateInfo* renderingInfo,
        VkPrimitiveTopology topology) -> VkPipeline {
        (void)attachmentHash;
        (void)renderPass;
        (void)renderingInfo;
        (void)topology;
        return VK_NULL_HANDLE;
    }

    FRhiVulkanComputePipeline::FRhiVulkanComputePipeline(
        const FRhiComputePipelineDesc& desc, VkDevice device)
        : FRhiPipeline(desc) {
        (void)device;
    }
    FRhiVulkanComputePipeline::~FRhiVulkanComputePipeline() = default;
    auto FRhiVulkanComputePipeline::GetNativePipeline() const noexcept -> VkPipeline {
        return VK_NULL_HANDLE;
    }
    auto FRhiVulkanComputePipeline::GetNativeLayout() const noexcept -> VkPipelineLayout {
        return VK_NULL_HANDLE;
    }

    FRhiVulkanBuffer::FRhiVulkanBuffer(const FRhiBufferDesc& desc, VkDevice device)
        : FRhiBuffer(desc) {
        (void)device;
    }
    FRhiVulkanBuffer::~FRhiVulkanBuffer() = default;
    auto FRhiVulkanBuffer::Lock(u64 offset, u64 size, ERhiBufferLockMode mode) -> FLockResult {
        (void)offset;
        (void)size;
        (void)mode;
        return {};
    }
    void FRhiVulkanBuffer::Unlock(FLockResult& lock) { (void)lock; }
    auto FRhiVulkanBuffer::GetNativeBuffer() const noexcept -> VkBuffer { return VK_NULL_HANDLE; }

    FRhiVulkanTexture::FRhiVulkanTexture(const FRhiTextureDesc& desc, VkDevice device)
        : FRhiTexture(desc) {
        (void)device;
    }
    FRhiVulkanTexture::FRhiVulkanTexture(const FRhiTextureDesc& desc, VkDevice device,
        VkImage image, VkImageView view, bool ownsImage)
        : FRhiTexture(desc) {
        (void)device;
        (void)image;
        (void)view;
        (void)ownsImage;
    }
    FRhiVulkanTexture::~FRhiVulkanTexture() = default;
    auto FRhiVulkanTexture::GetNativeImage() const noexcept -> VkImage { return VK_NULL_HANDLE; }
    auto FRhiVulkanTexture::GetDefaultView() const noexcept -> VkImageView {
        return VK_NULL_HANDLE;
    }

    FRhiVulkanSampler::FRhiVulkanSampler(const FRhiSamplerDesc& desc, VkDevice device)
        : FRhiSampler(desc) {
        (void)device;
    }
    FRhiVulkanSampler::~FRhiVulkanSampler() = default;
    auto FRhiVulkanSampler::GetNativeSampler() const noexcept -> VkSampler {
        return VK_NULL_HANDLE;
    }

    FRhiVulkanShader::FRhiVulkanShader(const FRhiShaderDesc& desc, VkDevice device)
        : FRhiShader(desc) {
        (void)device;
    }
    FRhiVulkanShader::~FRhiVulkanShader() = default;
    auto FRhiVulkanShader::GetModule() const noexcept -> VkShaderModule { return VK_NULL_HANDLE; }

    FRhiVulkanShaderResourceView::FRhiVulkanShaderResourceView(
        const FRhiShaderResourceViewDesc& desc, VkDevice device)
        : FRhiShaderResourceView(desc) {
        (void)device;
    }
    FRhiVulkanShaderResourceView::~FRhiVulkanShaderResourceView() = default;
    auto FRhiVulkanShaderResourceView::GetImageView() const noexcept -> VkImageView {
        return VK_NULL_HANDLE;
    }

    FRhiVulkanUnorderedAccessView::FRhiVulkanUnorderedAccessView(
        const FRhiUnorderedAccessViewDesc& desc, VkDevice device)
        : FRhiUnorderedAccessView(desc) {
        (void)device;
    }
    FRhiVulkanUnorderedAccessView::~FRhiVulkanUnorderedAccessView() = default;
    auto FRhiVulkanUnorderedAccessView::GetImageView() const noexcept -> VkImageView {
        return VK_NULL_HANDLE;
    }

    FRhiVulkanRenderTargetView::FRhiVulkanRenderTargetView(
        const FRhiRenderTargetViewDesc& desc, VkDevice device)
        : FRhiRenderTargetView(desc) {
        (void)device;
    }
    FRhiVulkanRenderTargetView::~FRhiVulkanRenderTargetView() = default;
    auto FRhiVulkanRenderTargetView::GetImageView() const noexcept -> VkImageView {
        return VK_NULL_HANDLE;
    }

    FRhiVulkanDepthStencilView::FRhiVulkanDepthStencilView(
        const FRhiDepthStencilViewDesc& desc, VkDevice device)
        : FRhiDepthStencilView(desc) {
        (void)device;
    }
    FRhiVulkanDepthStencilView::~FRhiVulkanDepthStencilView() = default;
    auto FRhiVulkanDepthStencilView::GetImageView() const noexcept -> VkImageView {
        return VK_NULL_HANDLE;
    }

    void FRhiVulkanCommandContext::RHIBeginTransition(const FRhiTransitionCreateInfo& info) {
        (void)info;
    }
    void FRhiVulkanCommandContext::RHIEndTransition(const FRhiTransitionCreateInfo& info) {
        (void)info;
    }
    void FRhiVulkanCommandContext::RHIClearColor(
        FRhiTexture* colorTarget, const FRhiClearColor& color) {
        (void)colorTarget;
        (void)color;
    }
    void FRhiVulkanCommandContext::RHISetBindGroup(
        u32 setIndex, FRhiBindGroup* group, const u32* dynamicOffsets, u32 dynamicOffsetCount) {
        (void)setIndex;
        (void)group;
        (void)dynamicOffsets;
        (void)dynamicOffsetCount;
    }
    void FRhiVulkanCommandContext::RHIDraw(
        u32 vertexCount, u32 instanceCount, u32 firstVertex, u32 firstInstance) {
        (void)vertexCount;
        (void)instanceCount;
        (void)firstVertex;
        (void)firstInstance;
    }
    void FRhiVulkanCommandContext::RHIDrawIndexed(
        u32 indexCount, u32 instanceCount, u32 firstIndex, i32 vertexOffset, u32 firstInstance) {
        (void)indexCount;
        (void)instanceCount;
        (void)firstIndex;
        (void)vertexOffset;
        (void)firstInstance;
    }
    void FRhiVulkanCommandContext::RHIDispatch(u32 groupCountX, u32 groupCountY, u32 groupCountZ) {
        (void)groupCountX;
        (void)groupCountY;
        (void)groupCountZ;
    }
} // namespace AltinaEngine::Rhi
