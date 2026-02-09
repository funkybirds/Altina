#include "RhiMock/RhiMockContext.h"

#include "Rhi/RhiBindGroup.h"
#include "Rhi/RhiBindGroupLayout.h"
#include "Rhi/RhiBuffer.h"
#include "Rhi/Command/RhiCmdContextOps.h"
#include "Rhi/RhiCommandContext.h"
#include "Rhi/RhiCommandList.h"
#include "Rhi/RhiCommandPool.h"
#include "Rhi/RhiFence.h"
#include "Rhi/RhiPipeline.h"
#include "Rhi/RhiPipelineLayout.h"
#include "Rhi/RhiQueue.h"
#include "Rhi/RhiSampler.h"
#include "Rhi/RhiSemaphore.h"
#include "Rhi/RhiShader.h"
#include "Rhi/RhiDevice.h"
#include "Rhi/RhiTexture.h"
#include "Rhi/RhiViewport.h"
#include "Container/SmartPtr.h"
#include "Types/Traits.h"
#include <type_traits>

namespace AltinaEngine::Rhi {
    namespace {
        template <typename TBase, typename TDerived, typename... Args>
        auto MakeSharedAs(Args&&... args) -> TShared<TBase> {
            using AllocatorType = Core::Container::TAllocator<TDerived>;
            using Traits        = Core::Container::TAllocatorTraits<AllocatorType>;

            static_assert(std::is_base_of_v<TBase, TDerived>,
                "MakeSharedAs requires TDerived to derive from TBase.");

            AllocatorType allocator;
            TDerived*     ptr = Traits::Allocate(allocator, 1);
            try {
                Traits::Construct(allocator, ptr, AltinaEngine::Forward<Args>(args)...);
            } catch (...) {
                Traits::Deallocate(allocator, ptr, 1);
                throw;
            }

            struct FDeleter {
                AllocatorType mAllocator;
                void          operator()(TBase* basePtr) {
                    if (!basePtr) {
                        return;
                    }
                    auto* derivedPtr = static_cast<TDerived*>(basePtr);
                    Traits::Destroy(mAllocator, derivedPtr);
                    Traits::Deallocate(mAllocator, derivedPtr, 1);
                }
            };

            return TShared<TBase>(ptr, FDeleter{ allocator });
        }

        class FRhiMockAdapter final : public FRhiAdapter {
        public:
            FRhiMockAdapter(const FRhiAdapterDesc& desc, const FRhiSupportedFeatures& features,
                const FRhiSupportedLimits& limits)
                : FRhiAdapter(desc), mFeatures(features), mLimits(limits) {}

            [[nodiscard]] auto GetFeatures() const noexcept -> const FRhiSupportedFeatures& {
                return mFeatures;
            }

            [[nodiscard]] auto GetLimits() const noexcept -> const FRhiSupportedLimits& {
                return mLimits;
            }

        private:
            FRhiSupportedFeatures mFeatures;
            FRhiSupportedLimits   mLimits;
        };

        class FRhiMockBuffer final : public FRhiBuffer {
        public:
            FRhiMockBuffer(const FRhiBufferDesc& desc, TShared<FRhiMockCounters> counters)
                : FRhiBuffer(desc), mCounters(AltinaEngine::Move(counters)) {
                if (mCounters) {
                    ++mCounters->mResourceCreated;
                }
            }

            ~FRhiMockBuffer() override {
                if (mCounters) {
                    ++mCounters->mResourceDestroyed;
                }
            }

        private:
            TShared<FRhiMockCounters> mCounters;
        };

        class FRhiMockTexture final : public FRhiTexture {
        public:
            FRhiMockTexture(const FRhiTextureDesc& desc, TShared<FRhiMockCounters> counters)
                : FRhiTexture(desc), mCounters(AltinaEngine::Move(counters)) {
                if (mCounters) {
                    ++mCounters->mResourceCreated;
                }
            }

            ~FRhiMockTexture() override {
                if (mCounters) {
                    ++mCounters->mResourceDestroyed;
                }
            }

        private:
            TShared<FRhiMockCounters> mCounters;
        };

        class FRhiMockViewport final : public FRhiViewport {
        public:
            FRhiMockViewport(const FRhiViewportDesc& desc, TShared<FRhiMockCounters> counters)
                : FRhiViewport(desc), mCounters(AltinaEngine::Move(counters)) {
                if (mCounters) {
                    ++mCounters->mResourceCreated;
                }
                CreateBackBuffer();
            }

            ~FRhiMockViewport() override {
                if (mCounters) {
                    ++mCounters->mResourceDestroyed;
                }
            }

            void Resize(u32 width, u32 height) override {
                UpdateExtent(width, height);
                CreateBackBuffer();
            }

            [[nodiscard]] auto GetBackBuffer() const noexcept -> FRhiTexture* override {
                return mBackBuffer.Get();
            }

            void Present(const FRhiPresentInfo& /*info*/) override {}

        private:
            void CreateBackBuffer() {
                FRhiTextureDesc texDesc{};
                texDesc.mWidth     = GetDesc().mWidth;
                texDesc.mHeight    = GetDesc().mHeight;
                texDesc.mFormat    = GetDesc().mFormat;
                texDesc.mBindFlags = ERhiTextureBindFlags::RenderTarget;
                if (!GetDesc().mDebugName.IsEmptyString()) {
                    texDesc.mDebugName = GetDesc().mDebugName;
                    texDesc.mDebugName.Append(TEXT(" BackBuffer"));
                }
                mBackBuffer = FRhiTextureRef::Adopt(new FRhiMockTexture(texDesc, mCounters));
            }

            FRhiTextureRef mBackBuffer;
            TShared<FRhiMockCounters> mCounters;
        };

        class FRhiMockSampler final : public FRhiSampler {
        public:
            FRhiMockSampler(const FRhiSamplerDesc& desc, TShared<FRhiMockCounters> counters)
                : FRhiSampler(desc), mCounters(AltinaEngine::Move(counters)) {
                if (mCounters) {
                    ++mCounters->mResourceCreated;
                }
            }

            ~FRhiMockSampler() override {
                if (mCounters) {
                    ++mCounters->mResourceDestroyed;
                }
            }

        private:
            TShared<FRhiMockCounters> mCounters;
        };

        class FRhiMockShader final : public FRhiShader {
        public:
            FRhiMockShader(const FRhiShaderDesc& desc, TShared<FRhiMockCounters> counters)
                : FRhiShader(desc), mCounters(AltinaEngine::Move(counters)) {
                if (mCounters) {
                    ++mCounters->mResourceCreated;
                }
            }

            ~FRhiMockShader() override {
                if (mCounters) {
                    ++mCounters->mResourceDestroyed;
                }
            }

        private:
            TShared<FRhiMockCounters> mCounters;
        };

        class FRhiMockGraphicsPipeline final : public FRhiPipeline {
        public:
            FRhiMockGraphicsPipeline(
                const FRhiGraphicsPipelineDesc& desc, TShared<FRhiMockCounters> counters)
                : FRhiPipeline(desc), mCounters(AltinaEngine::Move(counters)) {
                if (mCounters) {
                    ++mCounters->mResourceCreated;
                }
            }

            ~FRhiMockGraphicsPipeline() override {
                if (mCounters) {
                    ++mCounters->mResourceDestroyed;
                }
            }

        private:
            TShared<FRhiMockCounters> mCounters;
        };

        class FRhiMockComputePipeline final : public FRhiPipeline {
        public:
            FRhiMockComputePipeline(
                const FRhiComputePipelineDesc& desc, TShared<FRhiMockCounters> counters)
                : FRhiPipeline(desc), mCounters(AltinaEngine::Move(counters)) {
                if (mCounters) {
                    ++mCounters->mResourceCreated;
                }
            }

            ~FRhiMockComputePipeline() override {
                if (mCounters) {
                    ++mCounters->mResourceDestroyed;
                }
            }

        private:
            TShared<FRhiMockCounters> mCounters;
        };

        class FRhiMockPipelineLayout final : public FRhiPipelineLayout {
        public:
            FRhiMockPipelineLayout(
                const FRhiPipelineLayoutDesc& desc, TShared<FRhiMockCounters> counters)
                : FRhiPipelineLayout(desc), mCounters(AltinaEngine::Move(counters)) {
                if (mCounters) {
                    ++mCounters->mResourceCreated;
                }
            }

            ~FRhiMockPipelineLayout() override {
                if (mCounters) {
                    ++mCounters->mResourceDestroyed;
                }
            }

        private:
            TShared<FRhiMockCounters> mCounters;
        };

        class FRhiMockBindGroupLayout final : public FRhiBindGroupLayout {
        public:
            FRhiMockBindGroupLayout(
                const FRhiBindGroupLayoutDesc& desc, TShared<FRhiMockCounters> counters)
                : FRhiBindGroupLayout(desc), mCounters(AltinaEngine::Move(counters)) {
                if (mCounters) {
                    ++mCounters->mResourceCreated;
                }
            }

            ~FRhiMockBindGroupLayout() override {
                if (mCounters) {
                    ++mCounters->mResourceDestroyed;
                }
            }

        private:
            TShared<FRhiMockCounters> mCounters;
        };

        class FRhiMockBindGroup final : public FRhiBindGroup {
        public:
            FRhiMockBindGroup(const FRhiBindGroupDesc& desc, TShared<FRhiMockCounters> counters)
                : FRhiBindGroup(desc), mCounters(AltinaEngine::Move(counters)) {
                if (mCounters) {
                    ++mCounters->mResourceCreated;
                }
            }

            ~FRhiMockBindGroup() override {
                if (mCounters) {
                    ++mCounters->mResourceDestroyed;
                }
            }

        private:
            TShared<FRhiMockCounters> mCounters;
        };

        class FRhiMockFence final : public FRhiFence {
        public:
            FRhiMockFence(u64 initialValue, TShared<FRhiMockCounters> counters)
                : FRhiFence(), mValue(initialValue), mCounters(AltinaEngine::Move(counters)) {
                if (mCounters) {
                    ++mCounters->mResourceCreated;
                }
            }

            ~FRhiMockFence() override {
                if (mCounters) {
                    ++mCounters->mResourceDestroyed;
                }
            }

            [[nodiscard]] auto GetCompletedValue() const noexcept -> u64 override {
                return mValue;
            }
            void SignalCPU(u64 value) override { mValue = value; }
            void WaitCPU(u64 value) override { mValue = value; }
            void Reset(u64 value) override { mValue = value; }

        private:
            u64 mValue = 0ULL;
            TShared<FRhiMockCounters> mCounters;
        };

        class FRhiMockSemaphore final : public FRhiSemaphore {
        public:
            FRhiMockSemaphore(bool timeline, u64 initialValue, TShared<FRhiMockCounters> counters)
                : FRhiSemaphore(), mIsTimeline(timeline), mValue(initialValue),
                  mCounters(AltinaEngine::Move(counters)) {
                if (mCounters) {
                    ++mCounters->mResourceCreated;
                }
            }

            ~FRhiMockSemaphore() override {
                if (mCounters) {
                    ++mCounters->mResourceDestroyed;
                }
            }

            [[nodiscard]] auto IsTimeline() const noexcept -> bool override {
                return mIsTimeline;
            }
            [[nodiscard]] auto GetCurrentValue() const noexcept -> u64 override {
                return mValue;
            }
            void Signal(u64 value) {
                if (mIsTimeline) {
                    mValue = value;
                }
            }

        private:
            bool mIsTimeline = false;
            u64  mValue      = 0ULL;
            TShared<FRhiMockCounters> mCounters;
        };

        class FRhiMockCommandPool final : public FRhiCommandPool {
        public:
            FRhiMockCommandPool(
                const FRhiCommandPoolDesc& desc, TShared<FRhiMockCounters> counters)
                : FRhiCommandPool(desc), mCounters(AltinaEngine::Move(counters)) {
                if (mCounters) {
                    ++mCounters->mResourceCreated;
                }
            }

            ~FRhiMockCommandPool() override {
                if (mCounters) {
                    ++mCounters->mResourceDestroyed;
                }
            }

            void Reset() override {}

        private:
            TShared<FRhiMockCounters> mCounters;
        };

        class FRhiMockCommandList final : public FRhiCommandList {
        public:
            FRhiMockCommandList(const FRhiCommandListDesc& desc, TShared<FRhiMockCounters> counters)
                : FRhiCommandList(desc), mCounters(AltinaEngine::Move(counters)) {
                if (mCounters) {
                    ++mCounters->mResourceCreated;
                }
            }

            ~FRhiMockCommandList() override {
                if (mCounters) {
                    ++mCounters->mResourceDestroyed;
                }
            }

            void Reset(FRhiCommandPool* /*pool*/) override {}
            void Close() override {}

        private:
            TShared<FRhiMockCounters> mCounters;
        };

        class FRhiMockCommandContext final : public FRhiCommandContext,
                                             public IRhiCmdContextOps {
        public:
            FRhiMockCommandContext(const FRhiCommandContextDesc& desc,
                FRhiCommandListRef commandList, TShared<FRhiMockCounters> counters)
                : FRhiCommandContext(desc),
                  mCommandList(AltinaEngine::Move(commandList)),
                  mCounters(AltinaEngine::Move(counters)) {
                if (mCounters) {
                    ++mCounters->mResourceCreated;
                }
            }

            ~FRhiMockCommandContext() override {
                if (mCounters) {
                    ++mCounters->mResourceDestroyed;
                }
            }

            void Begin() override {}
            void End() override {}
            [[nodiscard]] auto GetCommandList() const noexcept -> FRhiCommandList* override {
                return mCommandList.Get();
            }

            void RHISetGraphicsPipeline(FRhiPipeline* /*pipeline*/) override {}
            void RHISetComputePipeline(FRhiPipeline* /*pipeline*/) override {}
            void RHISetPrimitiveTopology(ERhiPrimitiveTopology /*topology*/) override {}
            void RHISetVertexBuffer(u32 /*slot*/, const FRhiVertexBufferView& /*view*/) override {}
            void RHISetIndexBuffer(const FRhiIndexBufferView& /*view*/) override {}
            void RHISetViewport(const FRhiViewportRect& /*viewport*/) override {}
            void RHISetScissor(const FRhiScissorRect& /*scissor*/) override {}
            void RHISetRenderTargets(u32 /*colorTargetCount*/,
                FRhiTexture* const* /*colorTargets*/, FRhiTexture* /*depthTarget*/) override {}
            void RHIBeginRenderPass(const FRhiRenderPassDesc& /*desc*/) override {}
            void RHIEndRenderPass() override {}
            void RHIClearColor(FRhiTexture* /*colorTarget*/,
                const FRhiClearColor& /*color*/) override {}
            void RHISetBindGroup(u32 /*setIndex*/, FRhiBindGroup* /*group*/,
                const u32* /*dynamicOffsets*/, u32 /*dynamicOffsetCount*/) override {}
            void RHIDraw(u32 /*vertexCount*/, u32 /*instanceCount*/, u32 /*firstVertex*/,
                u32 /*firstInstance*/) override {}
            void RHIDrawIndexed(u32 /*indexCount*/, u32 /*instanceCount*/, u32 /*firstIndex*/,
                i32 /*vertexOffset*/, u32 /*firstInstance*/) override {}
            void RHIDispatch(u32 /*groupCountX*/, u32 /*groupCountY*/, u32 /*groupCountZ*/) override {}

        private:
            FRhiCommandListRef mCommandList;
            TShared<FRhiMockCounters> mCounters;
        };

        class FRhiMockQueue final : public FRhiQueue {
        public:
            explicit FRhiMockQueue(ERhiQueueType type) : FRhiQueue(type) {}

            void Submit(const FRhiSubmitInfo& info) override {
                if (info.mSignals) {
                    for (u32 i = 0; i < info.mSignalCount; ++i) {
                        const auto& signal = info.mSignals[i];
                        if (signal.mSemaphore == nullptr) {
                            continue;
                        }
                        if (!signal.mSemaphore->IsTimeline()) {
                            continue;
                        }
                        auto* mockSemaphore = static_cast<FRhiMockSemaphore*>(signal.mSemaphore);
                        mockSemaphore->Signal(signal.mValue);
                    }
                }

                if (info.mFence) {
                    info.mFence->SignalCPU(info.mFenceValue);
                }
            }

            void Signal(FRhiFence* fence, u64 value) override {
                if (fence) {
                    fence->SignalCPU(value);
                }
            }

            void Wait(FRhiFence* fence, u64 value) override {
                if (fence) {
                    fence->WaitCPU(value);
                }
            }

            void WaitIdle() override {}
            void Present(const FRhiPresentInfo& info) override {
                if (info.mViewport) {
                    info.mViewport->Present(info);
                }
            }
        };

        class FRhiMockDevice final : public FRhiDevice {
        public:
            FRhiMockDevice(const FRhiDeviceDesc& desc, const FRhiAdapterDesc& adapterDesc,
                const FRhiSupportedFeatures& features, const FRhiSupportedLimits& limits,
                TShared<FRhiMockCounters> counters)
                : FRhiDevice(desc, adapterDesc), mCounters(AltinaEngine::Move(counters)) {
                SetSupportedFeatures(features);
                SetSupportedLimits(limits);
                FRhiQueueCapabilities queueCaps;
                queueCaps.mSupportsGraphics = true;
                queueCaps.mSupportsCompute  = true;
                queueCaps.mSupportsCopy     = true;
                queueCaps.mSupportsAsyncCompute = false;
                queueCaps.mSupportsAsyncCopy    = false;
                SetQueueCapabilities(queueCaps);
                RegisterQueue(ERhiQueueType::Graphics,
                    MakeResource<FRhiMockQueue>(ERhiQueueType::Graphics));
                RegisterQueue(ERhiQueueType::Compute,
                    MakeResource<FRhiMockQueue>(ERhiQueueType::Compute));
                RegisterQueue(ERhiQueueType::Copy,
                    MakeResource<FRhiMockQueue>(ERhiQueueType::Copy));
                if (mCounters) {
                    ++mCounters->mDeviceCreated;
                }
            }

            ~FRhiMockDevice() override {
                if (mCounters) {
                    ++mCounters->mDeviceDestroyed;
                }
            }

            auto CreateBuffer(const FRhiBufferDesc& desc) -> FRhiBufferRef override {
                return MakeResource<FRhiMockBuffer>(desc, mCounters);
            }
            auto CreateTexture(const FRhiTextureDesc& desc) -> FRhiTextureRef override {
                return MakeResource<FRhiMockTexture>(desc, mCounters);
            }
            auto CreateViewport(const FRhiViewportDesc& desc) -> FRhiViewportRef override {
                return MakeResource<FRhiMockViewport>(desc, mCounters);
            }
            auto CreateSampler(const FRhiSamplerDesc& desc) -> FRhiSamplerRef override {
                return MakeResource<FRhiMockSampler>(desc, mCounters);
            }
            auto CreateShader(const FRhiShaderDesc& desc) -> FRhiShaderRef override {
                return MakeResource<FRhiMockShader>(desc, mCounters);
            }

            auto CreateGraphicsPipeline(const FRhiGraphicsPipelineDesc& desc)
                -> FRhiPipelineRef override {
                return MakeResource<FRhiMockGraphicsPipeline>(desc, mCounters);
            }
            auto CreateComputePipeline(const FRhiComputePipelineDesc& desc)
                -> FRhiPipelineRef override {
                return MakeResource<FRhiMockComputePipeline>(desc, mCounters);
            }
            auto CreatePipelineLayout(const FRhiPipelineLayoutDesc& desc)
                -> FRhiPipelineLayoutRef override {
                return MakeResource<FRhiMockPipelineLayout>(desc, mCounters);
            }

            auto CreateBindGroupLayout(const FRhiBindGroupLayoutDesc& desc)
                -> FRhiBindGroupLayoutRef override {
                return MakeResource<FRhiMockBindGroupLayout>(desc, mCounters);
            }
            auto CreateBindGroup(const FRhiBindGroupDesc& desc)
                -> FRhiBindGroupRef override {
                return MakeResource<FRhiMockBindGroup>(desc, mCounters);
            }

            auto CreateFence(u64 initialValue) -> FRhiFenceRef override {
                return MakeResource<FRhiMockFence>(initialValue, mCounters);
            }
            auto CreateSemaphore(bool timeline, u64 initialValue) -> FRhiSemaphoreRef override {
                return MakeResource<FRhiMockSemaphore>(timeline, initialValue, mCounters);
            }

            auto CreateCommandPool(const FRhiCommandPoolDesc& desc)
                -> FRhiCommandPoolRef override {
                return MakeResource<FRhiMockCommandPool>(desc, mCounters);
            }

            auto CreateCommandList(const FRhiCommandListDesc& desc)
                -> FRhiCommandListRef override {
                return MakeResource<FRhiMockCommandList>(desc, mCounters);
            }

            auto CreateCommandContext(const FRhiCommandContextDesc& desc)
                -> FRhiCommandContextRef override {
                FRhiCommandListDesc listDesc;
                listDesc.mDebugName = desc.mDebugName;
                listDesc.mQueueType = desc.mQueueType;
                listDesc.mListType  = desc.mListType;
                auto commandList = MakeResource<FRhiMockCommandList>(listDesc, mCounters);
                return MakeResource<FRhiMockCommandContext>(
                    desc, AltinaEngine::Move(commandList), mCounters);
            }

        private:
            TShared<FRhiMockCounters> mCounters;
        };
    } // namespace

    FRhiMockContext::FRhiMockContext()
        : mCounters(Core::Container::MakeShared<FRhiMockCounters>()) {}

    FRhiMockContext::~FRhiMockContext() { Shutdown(); }

    void FRhiMockContext::AddAdapter(
        const FRhiAdapterDesc& desc, const FRhiSupportedFeatures& features,
        const FRhiSupportedLimits& limits) {
        FRhiMockAdapterConfig config;
        config.mDesc     = desc;
        config.mFeatures = features;
        config.mLimits   = limits;
        mAdapterConfigs.PushBack(AltinaEngine::Move(config));
        InvalidateAdapterCache();
    }

    void FRhiMockContext::AddAdapter(const FRhiMockAdapterConfig& config) {
        mAdapterConfigs.PushBack(config);
        InvalidateAdapterCache();
    }

    void FRhiMockContext::SetAdapters(TVector<FRhiMockAdapterConfig> configs) {
        mAdapterConfigs = AltinaEngine::Move(configs);
        InvalidateAdapterCache();
    }

    void FRhiMockContext::ClearAdapters() {
        mAdapterConfigs.Clear();
        InvalidateAdapterCache();
    }

    void FRhiMockContext::MarkAdaptersDirty() { InvalidateAdapterCache(); }

    auto FRhiMockContext::GetCounters() const noexcept -> const FRhiMockCounters& {
        return *mCounters;
    }

    auto FRhiMockContext::GetInitializeCallCount() const noexcept -> u32 {
        return mCounters ? mCounters->mInitializeCalls : 0U;
    }

    auto FRhiMockContext::GetShutdownCallCount() const noexcept -> u32 {
        return mCounters ? mCounters->mShutdownCalls : 0U;
    }

    auto FRhiMockContext::GetEnumerateAdapterCallCount() const noexcept -> u32 {
        return mCounters ? mCounters->mEnumerateCalls : 0U;
    }

    auto FRhiMockContext::GetCreateDeviceCallCount() const noexcept -> u32 {
        return mCounters ? mCounters->mCreateDeviceCalls : 0U;
    }

    auto FRhiMockContext::GetDeviceCreatedCount() const noexcept -> u32 {
        return mCounters ? mCounters->mDeviceCreated : 0U;
    }

    auto FRhiMockContext::GetDeviceDestroyedCount() const noexcept -> u32 {
        return mCounters ? mCounters->mDeviceDestroyed : 0U;
    }

    auto FRhiMockContext::GetDeviceLiveCount() const noexcept -> u32 {
        return mCounters ? mCounters->GetDeviceLiveCount() : 0U;
    }

    auto FRhiMockContext::GetResourceCreatedCount() const noexcept -> u32 {
        return mCounters ? mCounters->mResourceCreated : 0U;
    }

    auto FRhiMockContext::GetResourceDestroyedCount() const noexcept -> u32 {
        return mCounters ? mCounters->mResourceDestroyed : 0U;
    }

    auto FRhiMockContext::GetResourceLiveCount() const noexcept -> u32 {
        return mCounters ? mCounters->GetResourceLiveCount() : 0U;
    }

    auto FRhiMockContext::InitializeBackend(const FRhiInitDesc& /*desc*/) -> bool {
        if (mCounters) {
            ++mCounters->mInitializeCalls;
        }
        return true;
    }

    void FRhiMockContext::ShutdownBackend() {
        if (mCounters) {
            ++mCounters->mShutdownCalls;
        }
    }

    void FRhiMockContext::EnumerateAdaptersInternal(TVector<TShared<FRhiAdapter>>& outAdapters) {
        if (mCounters) {
            ++mCounters->mEnumerateCalls;
        }

        outAdapters.Clear();
        outAdapters.Reserve(mAdapterConfigs.Size());

        for (const auto& config : mAdapterConfigs) {
            outAdapters.PushBack(
                MakeSharedAs<FRhiAdapter, FRhiMockAdapter>(config.mDesc, config.mFeatures,
                    config.mLimits));
        }
    }

    auto FRhiMockContext::CreateDeviceInternal(
        const TShared<FRhiAdapter>& adapter, const FRhiDeviceDesc& desc) -> TShared<FRhiDevice> {
        if (mCounters) {
            ++mCounters->mCreateDeviceCalls;
        }

        if (!adapter) {
            return {};
        }

        const auto* mockAdapter = static_cast<const FRhiMockAdapter*>(adapter.Get());
        const FRhiSupportedFeatures features =
            mockAdapter ? mockAdapter->GetFeatures() : FRhiSupportedFeatures{};
        const FRhiSupportedLimits limits =
            mockAdapter ? mockAdapter->GetLimits() : FRhiSupportedLimits{};

        return MakeSharedAs<FRhiDevice, FRhiMockDevice>(
            desc, adapter->GetDesc(), features, limits, mCounters);
    }

} // namespace AltinaEngine::Rhi
