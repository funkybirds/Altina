#pragma once

#include "RhiGeneralAPI.h"
#include "Rhi/RhiRefs.h"
#include "Rhi/RhiStructs.h"
#include "Rhi/RhiQueue.h"
#include "Rhi/RhiResourceDeleteQueue.h"
#include "Container/Vector.h"
#include "Types/NonCopyable.h"
#include "Types/Traits.h"

namespace AltinaEngine::Rhi {
    using Core::Container::FStringView;
    using Core::Container::TCountRef;
    using Core::Container::TVector;

    class AE_RHI_GENERAL_API FRhiDevice : public FNonCopyableClass {
    public:
        ~FRhiDevice() override = default;

        FRhiDevice(const FRhiDevice&) = delete;
        FRhiDevice(FRhiDevice&&) = delete;
        auto operator=(const FRhiDevice&) -> FRhiDevice& = delete;
        auto operator=(FRhiDevice&&) -> FRhiDevice& = delete;

        [[nodiscard]] auto GetDesc() const noexcept -> const FRhiDeviceDesc&;
        [[nodiscard]] auto GetAdapterDesc() const noexcept -> const FRhiAdapterDesc&;
        [[nodiscard]] auto GetDebugName() const noexcept -> FStringView;
        void               SetDebugName(FStringView name);

        [[nodiscard]] auto GetSupportedFeatures() const noexcept -> const FRhiSupportedFeatures&;
        [[nodiscard]] auto GetSupportedLimits() const noexcept -> const FRhiSupportedLimits&;
        [[nodiscard]] auto GetQueueCapabilities() const noexcept -> const FRhiQueueCapabilities&;
        [[nodiscard]] auto IsFeatureSupported(ERhiFeature feature) const noexcept -> bool;

        [[nodiscard]] auto GetQueue(ERhiQueueType type) const noexcept -> FRhiQueueRef;

        virtual auto       CreateBuffer(const FRhiBufferDesc& desc) -> FRhiBufferRef    = 0;
        virtual auto       CreateTexture(const FRhiTextureDesc& desc) -> FRhiTextureRef = 0;
        virtual auto       CreateViewport(const FRhiViewportDesc& desc) -> FRhiViewportRef = 0;
        virtual auto       CreateSampler(const FRhiSamplerDesc& desc) -> FRhiSamplerRef = 0;
        virtual auto       CreateShader(const FRhiShaderDesc& desc) -> FRhiShaderRef    = 0;

        virtual auto       CreateGraphicsPipeline(const FRhiGraphicsPipelineDesc& desc)
            -> FRhiPipelineRef = 0;
        virtual auto CreateComputePipeline(const FRhiComputePipelineDesc& desc)
            -> FRhiPipelineRef = 0;
        virtual auto CreatePipelineLayout(const FRhiPipelineLayoutDesc& desc)
            -> FRhiPipelineLayoutRef = 0;

        virtual auto CreateBindGroupLayout(const FRhiBindGroupLayoutDesc& desc)
            -> FRhiBindGroupLayoutRef                                                   = 0;
        virtual auto CreateBindGroup(const FRhiBindGroupDesc& desc) -> FRhiBindGroupRef = 0;

        virtual auto CreateFence(u64 initialValue) -> FRhiFenceRef                        = 0;
        virtual auto CreateSemaphore(bool timeline, u64 initialValue) -> FRhiSemaphoreRef = 0;

        virtual auto CreateCommandPool(const FRhiCommandPoolDesc& desc) -> FRhiCommandPoolRef = 0;
        virtual auto CreateCommandList(const FRhiCommandListDesc& desc) -> FRhiCommandListRef = 0;
        virtual auto CreateCommandContext(const FRhiCommandContextDesc& desc)
            -> FRhiCommandContextRef = 0;

        virtual void BeginFrame(u64 frameIndex);
        virtual void EndFrame();

        void ProcessResourceDeleteQueue(u64 completedSerial);
        void FlushResourceDeleteQueue();

    protected:
        FRhiDevice(const FRhiDeviceDesc& desc, const FRhiAdapterDesc& adapterDesc);

        void SetSupportedFeatures(const FRhiSupportedFeatures& features) noexcept;
        void SetSupportedLimits(const FRhiSupportedLimits& limits) noexcept;
        void SetQueueCapabilities(const FRhiQueueCapabilities& caps) noexcept;
        void RegisterQueue(ERhiQueueType type, FRhiQueueRef queue);

        template <typename TResource>
        auto AdoptResource(TResource* resource) -> TCountRef<TResource> {
            if (resource) {
                resource->SetDeleteQueue(&mResourceDeleteQueue);
            }
            return TCountRef<TResource>::Adopt(resource);
        }

        template <typename TResource, typename... Args>
        auto MakeResource(Args&&... args) -> TCountRef<TResource> {
            return AdoptResource(new TResource(AltinaEngine::Forward<Args>(args)...));
        }

    private:
        struct FRhiQueueEntry {
            ERhiQueueType mType  = ERhiQueueType::Graphics;
            FRhiQueueRef  mQueue = {};
        };

        void                    NormalizeDebugName();

        FRhiDeviceDesc          mDesc;
        FRhiAdapterDesc         mAdapterDesc;
        FRhiSupportedFeatures   mSupportedFeatures;
        FRhiSupportedLimits     mSupportedLimits;
        FRhiQueueCapabilities   mQueueCaps;
        TVector<FRhiQueueEntry> mQueues;
        FRhiResourceDeleteQueue mResourceDeleteQueue;
    };

} // namespace AltinaEngine::Rhi
