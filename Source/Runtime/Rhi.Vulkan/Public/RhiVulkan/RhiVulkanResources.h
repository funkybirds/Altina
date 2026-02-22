#pragma once

#include "RhiVulkanAPI.h"
#include "Rhi/RhiBuffer.h"
#include "Rhi/RhiSampler.h"
#include "Rhi/RhiShader.h"
#include "Rhi/RhiTexture.h"
#include "Rhi/RhiResourceView.h"

#if defined(AE_RHI_VULKAN_AVAILABLE) && AE_RHI_VULKAN_AVAILABLE
    #include <vulkan/vulkan.h>
#else
struct VkBuffer_T;
struct VkImage_T;
struct VkImageView_T;
struct VkSampler_T;
struct VkShaderModule_T;
using VkBuffer       = VkBuffer_T*;
using VkImage        = VkImage_T*;
using VkImageView    = VkImageView_T*;
using VkSampler      = VkSampler_T*;
using VkShaderModule = VkShaderModule_T*;
#endif

namespace AltinaEngine::Rhi {
    class AE_RHI_VULKAN_API FRhiVulkanBuffer final : public FRhiBuffer {
    public:
        FRhiVulkanBuffer(const FRhiBufferDesc& desc, VkDevice device);
        ~FRhiVulkanBuffer() override;

        auto Lock(u64 offset, u64 size, ERhiBufferLockMode mode) -> FLockResult override;
        void Unlock(FLockResult& lock) override;

        [[nodiscard]] auto GetNativeBuffer() const noexcept -> VkBuffer;

    private:
        struct FState;
        FState* mState = nullptr;
    };

    class AE_RHI_VULKAN_API FRhiVulkanTexture final : public FRhiTexture {
    public:
        FRhiVulkanTexture(const FRhiTextureDesc& desc, VkDevice device);
        FRhiVulkanTexture(const FRhiTextureDesc& desc, VkDevice device, VkImage image,
            VkImageView view, bool ownsImage);
        ~FRhiVulkanTexture() override;

        [[nodiscard]] auto GetNativeImage() const noexcept -> VkImage;
        [[nodiscard]] auto GetDefaultView() const noexcept -> VkImageView;

    private:
        struct FState;
        FState* mState = nullptr;
    };

    class AE_RHI_VULKAN_API FRhiVulkanSampler final : public FRhiSampler {
    public:
        FRhiVulkanSampler(const FRhiSamplerDesc& desc, VkDevice device);
        ~FRhiVulkanSampler() override;

        [[nodiscard]] auto GetNativeSampler() const noexcept -> VkSampler;

    private:
        struct FState;
        FState* mState = nullptr;
    };

    class AE_RHI_VULKAN_API FRhiVulkanShader final : public FRhiShader {
    public:
        FRhiVulkanShader(const FRhiShaderDesc& desc, VkDevice device);
        ~FRhiVulkanShader() override;

        [[nodiscard]] auto GetModule() const noexcept -> VkShaderModule;

    private:
        struct FState;
        FState* mState = nullptr;
    };

    class AE_RHI_VULKAN_API FRhiVulkanShaderResourceView final : public FRhiShaderResourceView {
    public:
        FRhiVulkanShaderResourceView(const FRhiShaderResourceViewDesc& desc, VkDevice device);
        ~FRhiVulkanShaderResourceView() override;

        [[nodiscard]] auto GetImageView() const noexcept -> VkImageView;

    private:
        struct FState;
        FState* mState = nullptr;
    };

    class AE_RHI_VULKAN_API FRhiVulkanUnorderedAccessView final : public FRhiUnorderedAccessView {
    public:
        FRhiVulkanUnorderedAccessView(const FRhiUnorderedAccessViewDesc& desc, VkDevice device);
        ~FRhiVulkanUnorderedAccessView() override;

        [[nodiscard]] auto GetImageView() const noexcept -> VkImageView;

    private:
        struct FState;
        FState* mState = nullptr;
    };

    class AE_RHI_VULKAN_API FRhiVulkanRenderTargetView final : public FRhiRenderTargetView {
    public:
        FRhiVulkanRenderTargetView(const FRhiRenderTargetViewDesc& desc, VkDevice device);
        ~FRhiVulkanRenderTargetView() override;

        [[nodiscard]] auto GetImageView() const noexcept -> VkImageView;

    private:
        struct FState;
        FState* mState = nullptr;
    };

    class AE_RHI_VULKAN_API FRhiVulkanDepthStencilView final : public FRhiDepthStencilView {
    public:
        FRhiVulkanDepthStencilView(const FRhiDepthStencilViewDesc& desc, VkDevice device);
        ~FRhiVulkanDepthStencilView() override;

        [[nodiscard]] auto GetImageView() const noexcept -> VkImageView;

    private:
        struct FState;
        FState* mState = nullptr;
    };

} // namespace AltinaEngine::Rhi
