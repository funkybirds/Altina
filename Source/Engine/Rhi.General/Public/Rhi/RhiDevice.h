#pragma once

#include "RhiGeneralAPI.h"
#include "Rhi/RhiFwd.h"
#include "Rhi/RhiStructs.h"
#include "Rhi/RhiQueue.h"
#include "Rhi/RhiResourceDeleteQueue.h"
#include "Container/CountRef.h"
#include "Container/Vector.h"
#include "Types/NonCopyable.h"

namespace AltinaEngine::Rhi {
    using Core::Container::FStringView;
    using Core::Container::TCountRef;
    using Core::Container::TVector;

    class AE_RHI_GENERAL_API FRhiDevice : public FNonCopyableClass {
    public:
        ~FRhiDevice() override = default;

        [[nodiscard]] auto GetDesc() const noexcept -> const FRhiDeviceDesc&;
        [[nodiscard]] auto GetAdapterDesc() const noexcept -> const FRhiAdapterDesc&;
        [[nodiscard]] auto GetDebugName() const noexcept -> FStringView;
        void               SetDebugName(FStringView name);

        [[nodiscard]] auto GetSupportedFeatures() const noexcept -> const FRhiSupportedFeatures&;
        [[nodiscard]] auto GetSupportedLimits() const noexcept -> const FRhiSupportedLimits&;
        [[nodiscard]] auto IsFeatureSupported(ERhiFeature feature) const noexcept -> bool;

        [[nodiscard]] auto GetQueue(ERhiQueueType type) const noexcept -> TCountRef<FRhiQueue>;

        virtual auto CreateBuffer(const FRhiBufferDesc& desc) -> TCountRef<FRhiBuffer> = 0;
        virtual auto CreateTexture(const FRhiTextureDesc& desc) -> TCountRef<FRhiTexture> = 0;
        virtual auto CreateSampler(const FRhiSamplerDesc& desc) -> TCountRef<FRhiSampler> = 0;
        virtual auto CreateShader(const FRhiShaderDesc& desc) -> TCountRef<FRhiShader> = 0;

        virtual auto CreateGraphicsPipeline(const FRhiGraphicsPipelineDesc& desc)
            -> TCountRef<FRhiPipeline> = 0;
        virtual auto CreateComputePipeline(const FRhiComputePipelineDesc& desc)
            -> TCountRef<FRhiPipeline> = 0;
        virtual auto CreatePipelineLayout(const FRhiPipelineLayoutDesc& desc)
            -> TCountRef<FRhiPipelineLayout> = 0;

        virtual auto CreateBindGroupLayout(const FRhiBindGroupLayoutDesc& desc)
            -> TCountRef<FRhiBindGroupLayout> = 0;
        virtual auto CreateBindGroup(const FRhiBindGroupDesc& desc)
            -> TCountRef<FRhiBindGroup> = 0;

        virtual auto CreateFence(bool signaled) -> TCountRef<FRhiFence> = 0;
        virtual auto CreateSemaphore() -> TCountRef<FRhiSemaphore>      = 0;

        virtual auto CreateCommandPool(const FRhiCommandPoolDesc& desc)
            -> TCountRef<FRhiCommandPool> = 0;

        void ProcessResourceDeleteQueue(u64 completedSerial);
        void FlushResourceDeleteQueue();

    protected:
        FRhiDevice(const FRhiDeviceDesc& desc, const FRhiAdapterDesc& adapterDesc);

        void SetSupportedFeatures(const FRhiSupportedFeatures& features) noexcept;
        void SetSupportedLimits(const FRhiSupportedLimits& limits) noexcept;
        void RegisterQueue(ERhiQueueType type, TCountRef<FRhiQueue> queue);

        template <typename TResource>
        auto AdoptResource(TResource* resource) -> TCountRef<TResource> {
            if (resource) {
                resource->SetDeleteQueue(&mResourceDeleteQueue);
            }
            return TCountRef<TResource>::Adopt(resource);
        }

    private:
        struct FRhiQueueEntry {
            ERhiQueueType       mType  = ERhiQueueType::Graphics;
            TCountRef<FRhiQueue> mQueue = {};
        };

        void NormalizeDebugName();

        FRhiDeviceDesc       mDesc;
        FRhiAdapterDesc      mAdapterDesc;
        FRhiSupportedFeatures mSupportedFeatures;
        FRhiSupportedLimits  mSupportedLimits;
        TVector<FRhiQueueEntry> mQueues;
        FRhiResourceDeleteQueue mResourceDeleteQueue;
    };

} // namespace AltinaEngine::Rhi
