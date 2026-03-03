#include "RhiVulkan/RhiVulkanPipeline.h"

#include "RhiVulkan/RhiVulkanDevice.h"
#include "RhiVulkan/RhiVulkanResources.h"
#include "RhiVulkanInternal.h"

#include "Rhi/RhiInit.h"

using AltinaEngine::Move;
namespace AltinaEngine::Rhi {
#if defined(AE_RHI_VULKAN_AVAILABLE) && AE_RHI_VULKAN_AVAILABLE
    namespace {
        [[nodiscard]] auto ToVkStageFlags(ERhiShaderStageFlags visibility) noexcept
            -> VkShaderStageFlags {
            VkShaderStageFlags out = 0;
            if (HasAnyFlags(visibility, ERhiShaderStageFlags::Vertex)) {
                out |= VK_SHADER_STAGE_VERTEX_BIT;
            }
            if (HasAnyFlags(visibility, ERhiShaderStageFlags::Pixel)) {
                out |= VK_SHADER_STAGE_FRAGMENT_BIT;
            }
            if (HasAnyFlags(visibility, ERhiShaderStageFlags::Compute)) {
                out |= VK_SHADER_STAGE_COMPUTE_BIT;
            }
            if (out == 0) {
                out = VK_SHADER_STAGE_ALL;
            }
            return out;
        }

        [[nodiscard]] auto ToVkDescriptorType(ERhiBindingType type, bool dynamicOffset) noexcept
            -> VkDescriptorType {
            switch (type) {
                case ERhiBindingType::ConstantBuffer:
                    return dynamicOffset ? VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC
                                         : VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
                case ERhiBindingType::SampledTexture:
                    return VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
                case ERhiBindingType::StorageTexture:
                    return VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
                case ERhiBindingType::Sampler:
                    return VK_DESCRIPTOR_TYPE_SAMPLER;
                case ERhiBindingType::SampledBuffer:
                case ERhiBindingType::StorageBuffer:
                    return VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
                default:
                    return VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
            }
        }

        [[nodiscard]] auto VkFormatSizeBytes(VkFormat format) noexcept -> u32 {
            switch (format) {
                case VK_FORMAT_R32_SFLOAT:
                    return 4U;
                case VK_FORMAT_R8G8B8A8_UNORM:
                case VK_FORMAT_R8G8B8A8_SRGB:
                case VK_FORMAT_B8G8R8A8_UNORM:
                case VK_FORMAT_B8G8R8A8_SRGB:
                    return 4U;
                case VK_FORMAT_R16G16B16A16_SFLOAT:
                    return 8U;
                default:
                    return 4U;
            }
        }
    } // namespace

    struct FRhiVulkanPipelineLayout::FState {
        VkDevice         mDevice = VK_NULL_HANDLE;
        VkPipelineLayout mLayout = VK_NULL_HANDLE;
        bool             mOwns   = true;
    };

    FRhiVulkanPipelineLayout::FRhiVulkanPipelineLayout(
        const FRhiPipelineLayoutDesc& desc, VkDevice device)
        : FRhiPipelineLayout(desc) {
        mState          = new FState{};
        mState->mDevice = device;

        Core::Container::TVector<VkDescriptorSetLayout> setLayouts;
        setLayouts.Reserve(desc.mBindGroupLayouts.Size());
        for (FRhiBindGroupLayout* layout : desc.mBindGroupLayouts) {
            auto* vkLayout = static_cast<FRhiVulkanBindGroupLayout*>(layout);
            setLayouts.PushBack(vkLayout ? vkLayout->GetNativeLayout() : VK_NULL_HANDLE);
        }

        Core::Container::TVector<VkPushConstantRange> pushConstants;
        pushConstants.Reserve(desc.mPushConstants.Size());
        for (const auto& range : desc.mPushConstants) {
            VkPushConstantRange pc{};
            pc.offset     = range.mOffset;
            pc.size       = range.mSize;
            pc.stageFlags = ToVkStageFlags(range.mVisibility);
            pushConstants.PushBack(pc);
        }

        VkPipelineLayoutCreateInfo info{};
        info.sType                  = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        info.setLayoutCount         = static_cast<u32>(setLayouts.Size());
        info.pSetLayouts            = setLayouts.IsEmpty() ? nullptr : setLayouts.Data();
        info.pushConstantRangeCount = static_cast<u32>(pushConstants.Size());
        info.pPushConstantRanges    = pushConstants.IsEmpty() ? nullptr : pushConstants.Data();

        if (vkCreatePipelineLayout(device, &info, nullptr, &mState->mLayout) != VK_SUCCESS) {
            mState->mLayout = VK_NULL_HANDLE;
        }
    }

    FRhiVulkanPipelineLayout::FRhiVulkanPipelineLayout(const FRhiPipelineLayoutDesc& desc,
        VkDevice device, VkPipelineLayout layout, bool ownsLayout)
        : FRhiPipelineLayout(desc) {
        mState          = new FState{};
        mState->mDevice = device;
        mState->mLayout = layout;
        mState->mOwns   = ownsLayout;
    }

    FRhiVulkanPipelineLayout::~FRhiVulkanPipelineLayout() {
        if (!mState) {
            return;
        }
        if (mState->mDevice && mState->mLayout && mState->mOwns) {
            vkDestroyPipelineLayout(mState->mDevice, mState->mLayout, nullptr);
        }
        delete mState;
        mState = nullptr;
    }

    auto FRhiVulkanPipelineLayout::GetNativeLayout() const noexcept -> VkPipelineLayout {
        return (mState != nullptr) ? mState->mLayout : VK_NULL_HANDLE;
    }

    struct FRhiVulkanBindGroupLayout::FState {
        struct FBindingMapEntry {
            u32             mSourceBinding = 0U;
            ERhiBindingType mType          = ERhiBindingType::StorageBuffer;
            bool            mDynamicOffset = false;
            u32             mVkBinding     = 0U;
        };

        VkDevice                                   mDevice = VK_NULL_HANDLE;
        VkDescriptorSetLayout                      mLayout = VK_NULL_HANDLE;
        bool                                       mOwns   = true;
        Core::Container::TVector<FBindingMapEntry> mBindingMap;
    };

    FRhiVulkanBindGroupLayout::FRhiVulkanBindGroupLayout(
        const FRhiBindGroupLayoutDesc& desc, VkDevice device)
        : FRhiBindGroupLayout(desc) {
        mState          = new FState{};
        mState->mDevice = device;

        Core::Container::TVector<VkDescriptorSetLayoutBinding> bindings;
        bindings.Reserve(desc.mEntries.Size());
        mState->mBindingMap.Reserve(desc.mEntries.Size());

        auto isVkBindingUsed = [&](u32 vkBinding) -> bool {
            for (const auto& existing : bindings) {
                if (existing.binding == vkBinding) {
                    return true;
                }
            }
            return false;
        };

        for (const auto& entry : desc.mEntries) {
            const VkDescriptorType descriptorType =
                ToVkDescriptorType(entry.mType, entry.mHasDynamicOffset);
            const VkShaderStageFlags stageFlags = ToVkStageFlags(entry.mVisibility);
            bool                     resolved   = false;

            for (const auto& mapped : mState->mBindingMap) {
                if (mapped.mSourceBinding != entry.mBinding || mapped.mType != entry.mType) {
                    continue;
                }
                for (auto& existing : bindings) {
                    if (existing.binding != mapped.mVkBinding) {
                        continue;
                    }
                    existing.stageFlags |= stageFlags;
                    if (entry.mArrayCount > existing.descriptorCount) {
                        existing.descriptorCount = entry.mArrayCount;
                    }
                    resolved = true;
                    break;
                }
                if (resolved) {
                    break;
                }
            }

            if (!resolved) {
                u32 vkBinding = entry.mBinding;
                if (isVkBindingUsed(vkBinding)) {
                    vkBinding = 0U;
                    while (isVkBindingUsed(vkBinding)) {
                        ++vkBinding;
                    }
                }

                VkDescriptorSetLayoutBinding binding{};
                binding.binding            = vkBinding;
                binding.descriptorType     = descriptorType;
                binding.descriptorCount    = entry.mArrayCount;
                binding.stageFlags         = stageFlags;
                binding.pImmutableSamplers = nullptr;
                bindings.PushBack(binding);

                FState::FBindingMapEntry mapped{};
                mapped.mSourceBinding = entry.mBinding;
                mapped.mType          = entry.mType;
                mapped.mDynamicOffset = entry.mHasDynamicOffset;
                mapped.mVkBinding     = vkBinding;
                mState->mBindingMap.PushBack(mapped);
            }
        }

        VkDescriptorSetLayoutCreateInfo info{};
        info.sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        info.bindingCount = static_cast<u32>(bindings.Size());
        info.pBindings    = bindings.IsEmpty() ? nullptr : bindings.Data();

        if (vkCreateDescriptorSetLayout(device, &info, nullptr, &mState->mLayout) != VK_SUCCESS) {
            mState->mLayout = VK_NULL_HANDLE;
        }
    }

    FRhiVulkanBindGroupLayout::FRhiVulkanBindGroupLayout(const FRhiBindGroupLayoutDesc& desc,
        VkDevice device, VkDescriptorSetLayout layout, bool ownsLayout)
        : FRhiBindGroupLayout(desc) {
        mState          = new FState{};
        mState->mDevice = device;
        mState->mLayout = layout;
        mState->mOwns   = ownsLayout;
    }

    FRhiVulkanBindGroupLayout::~FRhiVulkanBindGroupLayout() {
        if (!mState) {
            return;
        }
        if (mState->mDevice && mState->mLayout && mState->mOwns) {
            vkDestroyDescriptorSetLayout(mState->mDevice, mState->mLayout, nullptr);
        }
        delete mState;
        mState = nullptr;
    }

    auto FRhiVulkanBindGroupLayout::GetNativeLayout() const noexcept -> VkDescriptorSetLayout {
        return (mState != nullptr) ? mState->mLayout : VK_NULL_HANDLE;
    }

    auto FRhiVulkanBindGroupLayout::ResolveBinding(u32 sourceBinding, ERhiBindingType type,
        u32& outVkBinding, bool& outDynamicOffset) const noexcept -> bool {
        if (mState == nullptr) {
            return false;
        }
        for (const auto& mapped : mState->mBindingMap) {
            if (mapped.mSourceBinding == sourceBinding && mapped.mType == type) {
                outVkBinding     = mapped.mVkBinding;
                outDynamicOffset = mapped.mDynamicOffset;
                return true;
            }
        }
        return false;
    }

    struct FRhiVulkanBindGroup::FState {
        VkDevice         mDevice = VK_NULL_HANDLE;
        VkDescriptorPool mPool   = VK_NULL_HANDLE;
        VkDescriptorSet  mSet    = VK_NULL_HANDLE;
    };

    FRhiVulkanBindGroup::FRhiVulkanBindGroup(
        const FRhiBindGroupDesc& desc, VkDevice device, VkDescriptorSet set)
        : FRhiBindGroup(desc) {
        (void)set;
        mState          = new FState{};
        mState->mDevice = device;

        auto* vkLayout = static_cast<FRhiVulkanBindGroupLayout*>(desc.mLayout);
        if (!vkLayout || !device) {
            return;
        }

        // Create a small dedicated descriptor pool for this set.
        const auto&                                    layoutDesc = desc.mLayout->GetDesc();

        Core::Container::TVector<VkDescriptorPoolSize> poolSizes;
        poolSizes.Reserve(layoutDesc.mEntries.Size());

        auto addPoolSize = [&](VkDescriptorType type, u32 count) -> void {
            for (auto& ps : poolSizes) {
                if (ps.type == type) {
                    ps.descriptorCount += count;
                    return;
                }
            }
            VkDescriptorPoolSize size{};
            size.type            = type;
            size.descriptorCount = count;
            poolSizes.PushBack(size);
        };

        for (const auto& entry : layoutDesc.mEntries) {
            addPoolSize(
                ToVkDescriptorType(entry.mType, entry.mHasDynamicOffset), entry.mArrayCount);
        }

        VkDescriptorPoolCreateInfo poolInfo{};
        poolInfo.sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        poolInfo.maxSets       = 1;
        poolInfo.poolSizeCount = static_cast<u32>(poolSizes.Size());
        poolInfo.pPoolSizes    = poolSizes.IsEmpty() ? nullptr : poolSizes.Data();

        if (vkCreateDescriptorPool(device, &poolInfo, nullptr, &mState->mPool) != VK_SUCCESS) {
            mState->mPool = VK_NULL_HANDLE;
            return;
        }

        VkDescriptorSetLayout       nativeLayout = vkLayout->GetNativeLayout();
        VkDescriptorSetAllocateInfo alloc{};
        alloc.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        alloc.descriptorPool     = mState->mPool;
        alloc.descriptorSetCount = 1;
        alloc.pSetLayouts        = &nativeLayout;

        if (vkAllocateDescriptorSets(device, &alloc, &mState->mSet) != VK_SUCCESS) {
            mState->mSet = VK_NULL_HANDLE;
            return;
        }

        Core::Container::TVector<VkWriteDescriptorSet> writes;
        writes.Reserve(desc.mEntries.Size());

        Core::Container::TVector<VkDescriptorBufferInfo> bufferInfos;
        Core::Container::TVector<VkDescriptorImageInfo>  imageInfos;
        bufferInfos.Reserve(desc.mEntries.Size());
        imageInfos.Reserve(desc.mEntries.Size());

        for (const auto& entry : desc.mEntries) {
            u32  dstBinding    = entry.mBinding;
            bool dynamicOffset = false;
            if (!vkLayout
                || !vkLayout->ResolveBinding(
                    entry.mBinding, entry.mType, dstBinding, dynamicOffset)) {
                continue;
            }

            VkWriteDescriptorSet write{};
            write.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            write.dstSet          = mState->mSet;
            write.dstBinding      = dstBinding;
            write.dstArrayElement = entry.mArrayIndex;
            write.descriptorCount = 1;
            write.descriptorType  = ToVkDescriptorType(
                entry.mType, (entry.mType == ERhiBindingType::ConstantBuffer) && dynamicOffset);

            if (entry.mType == ERhiBindingType::ConstantBuffer
                || entry.mType == ERhiBindingType::SampledBuffer
                || entry.mType == ERhiBindingType::StorageBuffer) {
                auto* vkBuffer = static_cast<FRhiVulkanBuffer*>(entry.mBuffer);
                if (!vkBuffer) {
                    continue;
                }
                VkDescriptorBufferInfo bi{};
                bi.buffer = vkBuffer->GetNativeBuffer();
                bi.offset = static_cast<VkDeviceSize>(entry.mOffset);
                bi.range =
                    (entry.mSize == 0) ? VK_WHOLE_SIZE : static_cast<VkDeviceSize>(entry.mSize);
                bufferInfos.PushBack(bi);
                write.pBufferInfo = &bufferInfos.Back();
            } else if (entry.mType == ERhiBindingType::Sampler) {
                auto* vkSampler = static_cast<FRhiVulkanSampler*>(entry.mSampler);
                if (!vkSampler) {
                    continue;
                }
                VkDescriptorImageInfo ii{};
                ii.sampler = vkSampler->GetNativeSampler();
                imageInfos.PushBack(ii);
                write.pImageInfo = &imageInfos.Back();
            } else if (entry.mType == ERhiBindingType::SampledTexture
                || entry.mType == ERhiBindingType::StorageTexture) {
                auto* vkTex = static_cast<FRhiVulkanTexture*>(entry.mTexture);
                if (!vkTex) {
                    continue;
                }
                VkDescriptorImageInfo ii{};
                ii.imageView   = vkTex->GetDefaultView();
                ii.imageLayout = (entry.mType == ERhiBindingType::StorageTexture)
                    ? VK_IMAGE_LAYOUT_GENERAL
                    : VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
                imageInfos.PushBack(ii);
                write.pImageInfo = &imageInfos.Back();
            } else {
                continue;
            }

            writes.PushBack(write);
        }

        if (!writes.IsEmpty()) {
            vkUpdateDescriptorSets(
                device, static_cast<u32>(writes.Size()), writes.Data(), 0, nullptr);
        }
    }

    FRhiVulkanBindGroup::~FRhiVulkanBindGroup() {
        if (!mState) {
            return;
        }
        if (mState->mDevice && mState->mPool) {
            vkDestroyDescriptorPool(mState->mDevice, mState->mPool, nullptr);
        }
        delete mState;
        mState = nullptr;
    }

    auto FRhiVulkanBindGroup::GetDescriptorSet() const noexcept -> VkDescriptorSet {
        return (mState != nullptr) ? mState->mSet : VK_NULL_HANDLE;
    }

    struct FRhiVulkanGraphicsPipeline::FState {
        VkDevice         mDevice                       = VK_NULL_HANDLE;
        VkPipelineLayout mLayout                       = VK_NULL_HANDLE;
        bool             mSupportsExtendedDynamicState = false;

        struct FEntry {
            u64        mKey      = 0ULL;
            VkPipeline mPipeline = VK_NULL_HANDLE;
        };
        Core::Container::TVector<FEntry> mPipelines;
    };

    FRhiVulkanGraphicsPipeline::FRhiVulkanGraphicsPipeline(
        const FRhiGraphicsPipelineDesc& desc, VkDevice device, bool supportsExtendedDynamicState)
        : FRhiPipeline(desc) {
        mState                                = new FState{};
        mState->mDevice                       = device;
        mState->mSupportsExtendedDynamicState = supportsExtendedDynamicState;

        if (desc.mPipelineLayout) {
            mPipelineLayout = FRhiPipelineLayoutRef(desc.mPipelineLayout);
            auto* vkLayout  = static_cast<FRhiVulkanPipelineLayout*>(desc.mPipelineLayout);
            mState->mLayout = vkLayout ? vkLayout->GetNativeLayout() : VK_NULL_HANDLE;
        }
        if (desc.mVertexShader) {
            mVertexShader = FRhiShaderRef(desc.mVertexShader);
        }
        if (desc.mPixelShader) {
            mPixelShader = FRhiShaderRef(desc.mPixelShader);
        }
    }

    FRhiVulkanGraphicsPipeline::~FRhiVulkanGraphicsPipeline() {
        if (!mState) {
            return;
        }
        if (mState->mDevice) {
            for (auto& entry : mState->mPipelines) {
                if (entry.mPipeline) {
                    vkDestroyPipeline(mState->mDevice, entry.mPipeline, nullptr);
                }
            }
        }
        delete mState;
        mState = nullptr;
    }

    auto FRhiVulkanGraphicsPipeline::GetNativePipeline() const noexcept -> VkPipeline {
        // Graphics pipelines are attachment-dependent; the active pipeline is selected by command
        // context.
        return VK_NULL_HANDLE;
    }

    auto FRhiVulkanGraphicsPipeline::GetNativeLayout() const noexcept -> VkPipelineLayout {
        return (mState != nullptr) ? mState->mLayout : VK_NULL_HANDLE;
    }

    auto FRhiVulkanGraphicsPipeline::GetOrCreatePipeline(u64 attachmentHash,
        VkRenderPass renderPass, const VkPipelineRenderingCreateInfo* renderingInfo,
        VkPrimitiveTopology topology) -> VkPipeline {
        if (!mState || !mState->mDevice || !mState->mLayout) {
            return VK_NULL_HANDLE;
        }

        const VkPrimitiveTopology pipelineTopology =
            mState->mSupportsExtendedDynamicState ? VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST : topology;

        const u64 key = mState->mSupportsExtendedDynamicState
            ? attachmentHash
            : (attachmentHash ^ (static_cast<u64>(pipelineTopology) * 0x9E3779B97F4A7C15ULL));
        for (const auto& entry : mState->mPipelines) {
            if (entry.mKey == key) {
                return entry.mPipeline;
            }
        }

        auto* vs = static_cast<FRhiVulkanShader*>(mVertexShader.Get());
        auto* ps = static_cast<FRhiVulkanShader*>(mPixelShader.Get());
        if (!vs || !ps) {
            return VK_NULL_HANDLE;
        }

        VkPipelineShaderStageCreateInfo stages[2]{};
        stages[0].sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        stages[0].stage  = VK_SHADER_STAGE_VERTEX_BIT;
        stages[0].module = vs->GetModule();
        stages[0].pName  = "main";

        stages[1].sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        stages[1].stage  = VK_SHADER_STAGE_FRAGMENT_BIT;
        stages[1].module = ps->GetModule();
        stages[1].pName  = "main";

        // Vertex input (best-effort): assign locations sequentially and compute per-binding
        // strides.
        Core::Container::TVector<VkVertexInputAttributeDescription> attribs;
        Core::Container::TVector<VkVertexInputBindingDescription>   binds;
        Core::Container::TVector<u32>                               strideByBinding;

        const auto& layoutDesc = GetGraphicsDesc().mVertexLayout;
        for (usize i = 0; i < layoutDesc.mAttributes.Size(); ++i) {
            const auto&                       attr = layoutDesc.mAttributes[i];
            const VkFormat                    fmt  = Vulkan::Detail::ToVkFormat(attr.mFormat);
            VkVertexInputAttributeDescription a{};
            a.location = static_cast<u32>(i);
            a.binding  = attr.mInputSlot;
            a.format   = fmt;
            a.offset   = attr.mAlignedByteOffset;
            attribs.PushBack(a);

            const u32 neededStride = attr.mAlignedByteOffset + VkFormatSizeBytes(fmt);
            if (strideByBinding.Size() <= attr.mInputSlot) {
                const usize oldSize = strideByBinding.Size();
                strideByBinding.Resize(attr.mInputSlot + 1);
                for (usize s = oldSize; s < strideByBinding.Size(); ++s) {
                    strideByBinding[s] = 0U;
                }
            }
            if (strideByBinding[attr.mInputSlot] < neededStride) {
                strideByBinding[attr.mInputSlot] = neededStride;
            }
        }

        for (u32 binding = 0; binding < static_cast<u32>(strideByBinding.Size()); ++binding) {
            if (strideByBinding[binding] == 0U) {
                continue;
            }
            VkVertexInputBindingDescription b{};
            b.binding   = binding;
            b.stride    = strideByBinding[binding];
            b.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
            binds.PushBack(b);
        }

        VkPipelineVertexInputStateCreateInfo vertexInput{};
        vertexInput.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
        vertexInput.vertexBindingDescriptionCount   = static_cast<u32>(binds.Size());
        vertexInput.pVertexBindingDescriptions      = binds.IsEmpty() ? nullptr : binds.Data();
        vertexInput.vertexAttributeDescriptionCount = static_cast<u32>(attribs.Size());
        vertexInput.pVertexAttributeDescriptions    = attribs.IsEmpty() ? nullptr : attribs.Data();

        VkPipelineInputAssemblyStateCreateInfo inputAsm{};
        inputAsm.sType    = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
        inputAsm.topology = pipelineTopology;
        inputAsm.primitiveRestartEnable = VK_FALSE;

        VkPipelineViewportStateCreateInfo viewportState{};
        viewportState.sType         = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
        viewportState.viewportCount = 1;
        viewportState.scissorCount  = 1;

        VkPipelineRasterizationStateCreateInfo raster{};
        raster.sType                   = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
        raster.depthClampEnable        = VK_FALSE;
        raster.rasterizerDiscardEnable = VK_FALSE;
        raster.polygonMode             = VK_POLYGON_MODE_FILL;
        raster.cullMode  = Vulkan::Detail::ToVkCullMode(GetGraphicsDesc().mRasterState.mCullMode);
        raster.frontFace = Vulkan::Detail::ToVkFrontFace(GetGraphicsDesc().mRasterState.mFrontFace);
        raster.depthBiasEnable = VK_FALSE;
        raster.lineWidth       = 1.0f;

        VkPipelineMultisampleStateCreateInfo ms{};
        ms.sType                = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
        ms.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

        VkPipelineDepthStencilStateCreateInfo depth{};
        depth.sType            = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
        depth.depthTestEnable  = GetGraphicsDesc().mDepthState.mDepthEnable ? VK_TRUE : VK_FALSE;
        depth.depthWriteEnable = GetGraphicsDesc().mDepthState.mDepthWrite ? VK_TRUE : VK_FALSE;
        depth.depthCompareOp =
            Vulkan::Detail::ToVkCompareOp(GetGraphicsDesc().mDepthState.mDepthCompare);
        depth.depthBoundsTestEnable = VK_FALSE;
        depth.stencilTestEnable     = VK_FALSE;

        VkPipelineColorBlendAttachmentState blendAttachment{};
        blendAttachment.blendEnable =
            GetGraphicsDesc().mBlendState.mBlendEnable ? VK_TRUE : VK_FALSE;
        blendAttachment.srcColorBlendFactor =
            Vulkan::Detail::ToVkBlendFactor(GetGraphicsDesc().mBlendState.mSrcColor);
        blendAttachment.dstColorBlendFactor =
            Vulkan::Detail::ToVkBlendFactor(GetGraphicsDesc().mBlendState.mDstColor);
        blendAttachment.colorBlendOp =
            Vulkan::Detail::ToVkBlendOp(GetGraphicsDesc().mBlendState.mColorOp);
        blendAttachment.srcAlphaBlendFactor =
            Vulkan::Detail::ToVkBlendFactor(GetGraphicsDesc().mBlendState.mSrcAlpha);
        blendAttachment.dstAlphaBlendFactor =
            Vulkan::Detail::ToVkBlendFactor(GetGraphicsDesc().mBlendState.mDstAlpha);
        blendAttachment.alphaBlendOp =
            Vulkan::Detail::ToVkBlendOp(GetGraphicsDesc().mBlendState.mAlphaOp);
        blendAttachment.colorWriteMask =
            static_cast<VkColorComponentFlags>(GetGraphicsDesc().mBlendState.mColorWriteMask);

        VkPipelineColorBlendStateCreateInfo blend{};
        blend.sType           = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        blend.attachmentCount = 1;
        blend.pAttachments    = &blendAttachment;

        VkDynamicState dynStates[3]  = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
        u32            dynStateCount = 2U;
        if (mState->mSupportsExtendedDynamicState) {
    #if defined(VK_DYNAMIC_STATE_PRIMITIVE_TOPOLOGY_EXT)
            dynStates[dynStateCount++] = VK_DYNAMIC_STATE_PRIMITIVE_TOPOLOGY_EXT;
    #else
            dynStates[dynStateCount++] = VK_DYNAMIC_STATE_PRIMITIVE_TOPOLOGY;
    #endif
        }
        VkPipelineDynamicStateCreateInfo dyn{};
        dyn.sType             = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
        dyn.dynamicStateCount = dynStateCount;
        dyn.pDynamicStates    = dynStates;

        VkGraphicsPipelineCreateInfo info{};
        info.sType               = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
        info.stageCount          = 2;
        info.pStages             = stages;
        info.pVertexInputState   = &vertexInput;
        info.pInputAssemblyState = &inputAsm;
        info.pViewportState      = &viewportState;
        info.pRasterizationState = &raster;
        info.pMultisampleState   = &ms;
        info.pDepthStencilState  = &depth;
        info.pColorBlendState    = &blend;
        info.pDynamicState       = &dyn;
        info.layout              = mState->mLayout;
        info.renderPass          = renderPass;
        info.subpass             = 0;
        info.pNext               = renderingInfo;

        VkPipeline pipeline = VK_NULL_HANDLE;
        if (vkCreateGraphicsPipelines(mState->mDevice, VK_NULL_HANDLE, 1, &info, nullptr, &pipeline)
            != VK_SUCCESS) {
            return VK_NULL_HANDLE;
        }

        mState->mPipelines.PushBack(FState::FEntry{ key, pipeline });
        return pipeline;
    }

    struct FRhiVulkanComputePipeline::FState {
        VkDevice         mDevice   = VK_NULL_HANDLE;
        VkPipeline       mPipeline = VK_NULL_HANDLE;
        VkPipelineLayout mLayout   = VK_NULL_HANDLE;
    };

    FRhiVulkanComputePipeline::FRhiVulkanComputePipeline(
        const FRhiComputePipelineDesc& desc, VkDevice device)
        : FRhiPipeline(desc) {
        mState          = new FState{};
        mState->mDevice = device;

        if (desc.mPipelineLayout) {
            mPipelineLayout = FRhiPipelineLayoutRef(desc.mPipelineLayout);
            auto* vkLayout  = static_cast<FRhiVulkanPipelineLayout*>(desc.mPipelineLayout);
            mState->mLayout = vkLayout ? vkLayout->GetNativeLayout() : VK_NULL_HANDLE;
        }
        if (desc.mComputeShader) {
            mComputeShader = FRhiShaderRef(desc.mComputeShader);
        }

        auto* cs = static_cast<FRhiVulkanShader*>(mComputeShader.Get());
        if (!cs || !mState->mLayout) {
            return;
        }

        VkPipelineShaderStageCreateInfo stage{};
        stage.sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        stage.stage  = VK_SHADER_STAGE_COMPUTE_BIT;
        stage.module = cs->GetModule();
        stage.pName  = "main";

        VkComputePipelineCreateInfo info{};
        info.sType  = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
        info.stage  = stage;
        info.layout = mState->mLayout;

        if (vkCreateComputePipelines(device, VK_NULL_HANDLE, 1, &info, nullptr, &mState->mPipeline)
            != VK_SUCCESS) {
            mState->mPipeline = VK_NULL_HANDLE;
        }
    }

    FRhiVulkanComputePipeline::~FRhiVulkanComputePipeline() {
        if (!mState) {
            return;
        }
        if (mState->mDevice && mState->mPipeline) {
            vkDestroyPipeline(mState->mDevice, mState->mPipeline, nullptr);
        }
        delete mState;
        mState = nullptr;
    }

    auto FRhiVulkanComputePipeline::GetNativePipeline() const noexcept -> VkPipeline {
        return (mState != nullptr) ? mState->mPipeline : VK_NULL_HANDLE;
    }

    auto FRhiVulkanComputePipeline::GetNativeLayout() const noexcept -> VkPipelineLayout {
        return (mState != nullptr) ? mState->mLayout : VK_NULL_HANDLE;
    }

#else
    // Compiled out when Vulkan is unavailable. Stubs live in RhiVulkanStubs.cpp.
#endif
} // namespace AltinaEngine::Rhi
