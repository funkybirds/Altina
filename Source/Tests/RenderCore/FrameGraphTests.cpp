#include "TestHarness.h"

#include "FrameGraph/FrameGraph.h"
#include "FrameGraph/FrameGraphExecutor.h"
#include "RhiMock/RhiMockContext.h"
#include "Rhi/Command/RhiCmdContext.h"
#include "Rhi/RhiCommandContext.h"
#include "Rhi/RhiCommandPool.h"
#include "Rhi/RhiBuffer.h"
#include "Rhi/RhiBindGroup.h"
#include "Rhi/RhiBindGroupLayout.h"
#include "Rhi/RhiDevice.h"
#include "Rhi/RhiFence.h"
#include "Rhi/RhiPipeline.h"
#include "Rhi/RhiPipelineLayout.h"
#include "Rhi/RhiSampler.h"
#include "Rhi/RhiShader.h"
#include "Rhi/RhiTexture.h"
#include "Rhi/RhiStructs.h"
#include "Rhi/RhiTransition.h"
#include "Rhi/RhiCommandList.h"
#include "Rhi/RhiViewport.h"

namespace {
    using AltinaEngine::i32;
    using AltinaEngine::TChar;
    using AltinaEngine::u32;
    using AltinaEngine::u64;
    using AltinaEngine::u8;
    using AltinaEngine::RenderCore::EFrameGraphPassType;
    using AltinaEngine::RenderCore::EFrameGraphQueue;
    using AltinaEngine::RenderCore::FFrameGraph;
    using AltinaEngine::RenderCore::FFrameGraphBufferDesc;
    using AltinaEngine::RenderCore::FFrameGraphBufferRef;
    using AltinaEngine::RenderCore::FFrameGraphDSVRef;
    using AltinaEngine::RenderCore::FFrameGraphExecutor;
    using AltinaEngine::RenderCore::FFrameGraphPassBuilder;
    using AltinaEngine::RenderCore::FFrameGraphPassDesc;
    using AltinaEngine::RenderCore::FFrameGraphPassResources;
    using AltinaEngine::RenderCore::FFrameGraphRTVRef;
    using AltinaEngine::RenderCore::FFrameGraphSRVRef;
    using AltinaEngine::RenderCore::FFrameGraphTextureDesc;
    using AltinaEngine::RenderCore::FFrameGraphTextureRef;
    using AltinaEngine::RenderCore::FFrameGraphUAVRef;
    using AltinaEngine::RenderCore::FRdgDepthStencilBinding;
    using AltinaEngine::RenderCore::FRdgRenderTargetBinding;
    using AltinaEngine::Rhi::ERhiAdapterType;
    using AltinaEngine::Rhi::ERhiBufferBindFlags;
    using AltinaEngine::Rhi::ERhiFormat;
    using AltinaEngine::Rhi::ERhiGpuPreference;
    using AltinaEngine::Rhi::ERhiResourceState;
    using AltinaEngine::Rhi::ERhiTextureBindFlags;
    using AltinaEngine::Rhi::ERhiVendorId;
    using AltinaEngine::Rhi::FRhiAdapterDesc;
    using AltinaEngine::Rhi::FRhiBufferDesc;
    using AltinaEngine::Rhi::FRhiBufferViewRange;
    using AltinaEngine::Rhi::FRhiDepthStencilViewDesc;
    using AltinaEngine::Rhi::FRhiInitDesc;
    using AltinaEngine::Rhi::FRhiMockContext;
    using AltinaEngine::Rhi::FRhiRenderTargetViewDesc;
    using AltinaEngine::Rhi::FRhiShaderResourceViewDesc;
    using AltinaEngine::Rhi::FRhiTextureDesc;
    using AltinaEngine::Rhi::FRhiTextureViewRange;
    using AltinaEngine::Rhi::FRhiUnorderedAccessViewDesc;

    auto MakeAdapterDesc(const TChar* name, ERhiAdapterType type, ERhiVendorId vendor)
        -> FRhiAdapterDesc {
        FRhiAdapterDesc desc;
        desc.mName.Assign(name);
        desc.mType     = type;
        desc.mVendorId = vendor;
        return desc;
    }

    auto CreateMockDevice(FRhiMockContext& context) {
        context.AddAdapter(MakeAdapterDesc(
            TEXT("Mock Discrete"), ERhiAdapterType::Discrete, ERhiVendorId::Nvidia));
        FRhiInitDesc initDesc;
        initDesc.mAdapterPreference = ERhiGpuPreference::HighPerformance;
        REQUIRE(context.Init(initDesc));
        auto device = context.CreateDevice(0);
        REQUIRE(device);
        return device;
    }

    class FTestCmdContext final : public AltinaEngine::Rhi::FRhiCmdContext {
    public:
        void RHIUpdateDynamicBufferDiscard(AltinaEngine::Rhi::FRhiBuffer* /*buffer*/,
            const void* /*data*/, u64 /*sizeBytes*/, u64 /*offsetBytes*/) override {}

        void RHISetGraphicsPipeline(AltinaEngine::Rhi::FRhiPipeline* /*pipeline*/) override {}
        void RHISetComputePipeline(AltinaEngine::Rhi::FRhiPipeline* /*pipeline*/) override {}
        void RHISetPrimitiveTopology(
            AltinaEngine::Rhi::ERhiPrimitiveTopology /*topology*/) override {}
        void RHISetVertexBuffer(
            u32 /*slot*/, const AltinaEngine::Rhi::FRhiVertexBufferView& /*view*/) override {}
        void RHISetIndexBuffer(const AltinaEngine::Rhi::FRhiIndexBufferView& /*view*/) override {}
        void RHISetViewport(const AltinaEngine::Rhi::FRhiViewportRect& /*viewport*/) override {}
        void RHISetScissor(const AltinaEngine::Rhi::FRhiScissorRect& /*scissor*/) override {}
        void RHISetRenderTargets(u32 /*colorTargetCount*/,
            AltinaEngine::Rhi::FRhiTexture* const* /*colorTargets*/,
            AltinaEngine::Rhi::FRhiTexture* /*depthTarget*/) override {}
        void RHIBeginRenderPass(const AltinaEngine::Rhi::FRhiRenderPassDesc& /*desc*/) override {}
        void RHIEndRenderPass() override {}
        void RHIBeginTransition(
            const AltinaEngine::Rhi::FRhiTransitionCreateInfo& /*info*/) override {}
        void RHIEndTransition(
            const AltinaEngine::Rhi::FRhiTransitionCreateInfo& /*info*/) override {}
        void RHIClearColor(AltinaEngine::Rhi::FRhiTexture* /*colorTarget*/,
            const AltinaEngine::Rhi::FRhiClearColor& /*color*/) override {}
        void RHISetBindGroup(u32 /*setIndex*/, AltinaEngine::Rhi::FRhiBindGroup* /*group*/,
            const u32* /*dynamicOffsets*/, u32 /*dynamicOffsetCount*/) override {}
        void RHIDraw(u32 /*vertexCount*/, u32 /*instanceCount*/, u32 /*firstVertex*/,
            u32 /*firstInstance*/) override {}
        void RHIDrawIndexed(u32 /*indexCount*/, u32 /*instanceCount*/, u32 /*firstIndex*/,
            i32 /*vertexOffset*/, u32 /*firstInstance*/) override {}
        void RHIDispatch(u32 /*groupCountX*/, u32 /*groupCountY*/, u32 /*groupCountZ*/) override {}
    };

    enum class ETestEvent : u8 {
        BeginTransition = 0,
        EndTransition,
        Dispatch
    };

    struct FSubmitRecord {
        AltinaEngine::Rhi::ERhiQueueType mQueue = AltinaEngine::Rhi::ERhiQueueType::Graphics;
        AltinaEngine::Core::Container::TVector<AltinaEngine::Rhi::FRhiQueueWait>   mWaits;
        AltinaEngine::Core::Container::TVector<AltinaEngine::Rhi::FRhiQueueSignal> mSignals;
    };

    class FTestSemaphore final : public AltinaEngine::Rhi::FRhiSemaphore {
    public:
        explicit FTestSemaphore(bool timeline, u64 initialValue)
            : FRhiSemaphore(), mTimeline(timeline), mValue(initialValue) {}

        [[nodiscard]] auto IsTimeline() const noexcept -> bool override { return mTimeline; }
        [[nodiscard]] auto GetCurrentValue() const noexcept -> u64 override { return mValue; }
        void               Signal(u64 value) { mValue = value; }

    private:
        bool mTimeline = true;
        u64  mValue    = 0ULL;
    };

    class FTestFence final : public AltinaEngine::Rhi::FRhiFence {
    public:
        explicit FTestFence(u64 initialValue) : FRhiFence(), mValue(initialValue) {}

        [[nodiscard]] auto GetCompletedValue() const noexcept -> u64 override { return mValue; }
        void               SignalCPU(u64 value) override { mValue = value; }
        void               WaitCPU(u64 value) override { mValue = value; }
        void               Reset(u64 value) override { mValue = value; }

    private:
        u64 mValue = 0ULL;
    };

    class FTestTransition final : public AltinaEngine::Rhi::FRhiTransition {
    public:
        FTestTransition(const AltinaEngine::Rhi::FRhiTransitionDesc& desc,
            AltinaEngine::Rhi::FRhiSemaphoreRef                      semaphore)
            : FRhiTransition(desc), mSemaphore(Move(semaphore)) {}

        [[nodiscard]] auto GetSemaphore() const noexcept
            -> AltinaEngine::Rhi::FRhiSemaphore* override {
            return mSemaphore.Get();
        }
        [[nodiscard]] auto GetSignalValue() const noexcept -> u64 override { return mSignalValue; }
        void               SetSignalValue(u64 value) override { mSignalValue = value; }

    private:
        AltinaEngine::Rhi::FRhiSemaphoreRef mSemaphore;
        u64                                 mSignalValue = 0ULL;
    };

    class FTestCommandList final : public AltinaEngine::Rhi::FRhiCommandList {
    public:
        explicit FTestCommandList(const AltinaEngine::Rhi::FRhiCommandListDesc& desc)
            : FRhiCommandList(desc) {}

        void Reset(AltinaEngine::Rhi::FRhiCommandPool* /*pool*/) override {}
        void Close() override {}
    };

    class FTestCommandContext final : public AltinaEngine::Rhi::FRhiCommandContext {
    public:
        FTestCommandContext(const AltinaEngine::Rhi::FRhiCommandContextDesc& desc,
            AltinaEngine::Rhi::FRhiQueue*                                    queue)
            : FRhiCommandContext(desc), mQueue(queue) {}

        auto RHISubmitActiveSection(
            const AltinaEngine::Rhi::FRhiCommandContextSubmitInfo& submitInfo)
            -> AltinaEngine::Rhi::FRhiCommandSubmissionStamp override {
            if (mQueue != nullptr) {
                AltinaEngine::Rhi::FRhiSubmitInfo submit{};
                submit.mWaits       = submitInfo.mWaits;
                submit.mWaitCount   = submitInfo.mWaitCount;
                submit.mSignals     = submitInfo.mSignals;
                submit.mSignalCount = submitInfo.mSignalCount;
                submit.mFence       = submitInfo.mFence;
                submit.mFenceValue  = submitInfo.mFenceValue;
                mQueue->Submit(submit);
            }
            AltinaEngine::Rhi::FRhiCommandSubmissionStamp stamp{};
            stamp.mSerial = 1ULL;
            return stamp;
        }

        const AltinaEngine::Core::Container::TVector<ETestEvent>& GetEvents() const noexcept {
            return mEvents;
        }

        void RHIUpdateDynamicBufferDiscard(AltinaEngine::Rhi::FRhiBuffer* /*buffer*/,
            const void* /*data*/, u64 /*sizeBytes*/, u64 /*offsetBytes*/) override {}

        void RHISetGraphicsPipeline(AltinaEngine::Rhi::FRhiPipeline* /*pipeline*/) override {}
        void RHISetComputePipeline(AltinaEngine::Rhi::FRhiPipeline* /*pipeline*/) override {}
        void RHISetPrimitiveTopology(
            AltinaEngine::Rhi::ERhiPrimitiveTopology /*topology*/) override {}
        void RHISetVertexBuffer(
            u32 /*slot*/, const AltinaEngine::Rhi::FRhiVertexBufferView& /*view*/) override {}
        void RHISetIndexBuffer(const AltinaEngine::Rhi::FRhiIndexBufferView& /*view*/) override {}
        void RHISetViewport(const AltinaEngine::Rhi::FRhiViewportRect& /*viewport*/) override {}
        void RHISetScissor(const AltinaEngine::Rhi::FRhiScissorRect& /*scissor*/) override {}
        void RHISetRenderTargets(u32 /*colorTargetCount*/,
            AltinaEngine::Rhi::FRhiTexture* const* /*colorTargets*/,
            AltinaEngine::Rhi::FRhiTexture* /*depthTarget*/) override {}
        void RHIBeginRenderPass(const AltinaEngine::Rhi::FRhiRenderPassDesc& /*desc*/) override {}
        void RHIEndRenderPass() override {}
        void RHIBeginTransition(
            const AltinaEngine::Rhi::FRhiTransitionCreateInfo& /*info*/) override {
            mEvents.PushBack(ETestEvent::BeginTransition);
        }
        void RHIEndTransition(
            const AltinaEngine::Rhi::FRhiTransitionCreateInfo& /*info*/) override {
            mEvents.PushBack(ETestEvent::EndTransition);
        }
        void RHIClearColor(AltinaEngine::Rhi::FRhiTexture* /*colorTarget*/,
            const AltinaEngine::Rhi::FRhiClearColor& /*color*/) override {}
        void RHISetBindGroup(u32 /*setIndex*/, AltinaEngine::Rhi::FRhiBindGroup* /*group*/,
            const u32* /*dynamicOffsets*/, u32 /*dynamicOffsetCount*/) override {}
        void RHIDraw(u32 /*vertexCount*/, u32 /*instanceCount*/, u32 /*firstVertex*/,
            u32 /*firstInstance*/) override {}
        void RHIDrawIndexed(u32 /*indexCount*/, u32 /*instanceCount*/, u32 /*firstIndex*/,
            i32 /*vertexOffset*/, u32 /*firstInstance*/) override {}
        void RHIDispatch(u32 /*groupCountX*/, u32 /*groupCountY*/, u32 /*groupCountZ*/) override {
            mEvents.PushBack(ETestEvent::Dispatch);
        }

    private:
        AltinaEngine::Rhi::FRhiQueue*                      mQueue = nullptr;
        AltinaEngine::Core::Container::TVector<ETestEvent> mEvents;
    };

    class FTestQueue final : public AltinaEngine::Rhi::FRhiQueue {
    public:
        explicit FTestQueue(AltinaEngine::Rhi::ERhiQueueType type) : FRhiQueue(type) {}

        void Submit(const AltinaEngine::Rhi::FRhiSubmitInfo& info) override {
            FSubmitRecord record{};
            record.mQueue = GetType();
            if (info.mWaits && info.mWaitCount > 0U) {
                record.mWaits.Reserve(info.mWaitCount);
                for (u32 i = 0U; i < info.mWaitCount; ++i) {
                    record.mWaits.PushBack(info.mWaits[i]);
                }
            }
            if (info.mSignals && info.mSignalCount > 0U) {
                record.mSignals.Reserve(info.mSignalCount);
                for (u32 i = 0U; i < info.mSignalCount; ++i) {
                    record.mSignals.PushBack(info.mSignals[i]);
                }
            }
            mSubmits.PushBack(record);
            if (info.mSignals) {
                for (u32 i = 0U; i < info.mSignalCount; ++i) {
                    if (info.mSignals[i].mSemaphore && info.mSignals[i].mSemaphore->IsTimeline()) {
                        auto* sem = static_cast<FTestSemaphore*>(info.mSignals[i].mSemaphore);
                        if (sem) {
                            sem->Signal(info.mSignals[i].mValue);
                        }
                    }
                }
            }
        }

        void Signal(AltinaEngine::Rhi::FRhiFence* fence, u64 value) override {
            if (fence) {
                fence->SignalCPU(value);
            }
        }
        void Wait(AltinaEngine::Rhi::FRhiFence* fence, u64 value) override {
            if (fence) {
                fence->WaitCPU(value);
            }
        }
        void WaitIdle() override {}
        void Present(const AltinaEngine::Rhi::FRhiPresentInfo& /*info*/) override {}

        const AltinaEngine::Core::Container::TVector<FSubmitRecord>& GetSubmits() const noexcept {
            return mSubmits;
        }

    private:
        AltinaEngine::Core::Container::TVector<FSubmitRecord> mSubmits;
    };

    class FTestDevice final : public AltinaEngine::Rhi::FRhiDevice {
    public:
        FTestDevice(bool supportsAsyncCompute, bool supportsAsyncCopy)
            : FRhiDevice(
                  AltinaEngine::Rhi::FRhiDeviceDesc{}, AltinaEngine::Rhi::FRhiAdapterDesc{}) {
            AltinaEngine::Rhi::FRhiQueueCapabilities caps{};
            caps.mSupportsGraphics     = true;
            caps.mSupportsCompute      = true;
            caps.mSupportsCopy         = true;
            caps.mSupportsAsyncCompute = supportsAsyncCompute;
            caps.mSupportsAsyncCopy    = supportsAsyncCopy;
            SetQueueCapabilities(caps);

            RegisterQueue(AltinaEngine::Rhi::ERhiQueueType::Graphics,
                MakeResource<FTestQueue>(AltinaEngine::Rhi::ERhiQueueType::Graphics));
            RegisterQueue(AltinaEngine::Rhi::ERhiQueueType::Compute,
                MakeResource<FTestQueue>(AltinaEngine::Rhi::ERhiQueueType::Compute));
            RegisterQueue(AltinaEngine::Rhi::ERhiQueueType::Copy,
                MakeResource<FTestQueue>(AltinaEngine::Rhi::ERhiQueueType::Copy));
        }

        auto CreateBuffer(const AltinaEngine::Rhi::FRhiBufferDesc& desc)
            -> AltinaEngine::Rhi::FRhiBufferRef override {
            return MakeResource<AltinaEngine::Rhi::FRhiBuffer>(desc);
        }
        auto CreateTexture(const AltinaEngine::Rhi::FRhiTextureDesc& desc)
            -> AltinaEngine::Rhi::FRhiTextureRef override {
            return MakeResource<AltinaEngine::Rhi::FRhiTexture>(desc);
        }
        auto CreateViewport(const AltinaEngine::Rhi::FRhiViewportDesc& /*desc*/)
            -> AltinaEngine::Rhi::FRhiViewportRef override {
            return {};
        }
        auto CreateSampler(const AltinaEngine::Rhi::FRhiSamplerDesc& /*desc*/)
            -> AltinaEngine::Rhi::FRhiSamplerRef override {
            return {};
        }
        auto CreateShader(const AltinaEngine::Rhi::FRhiShaderDesc& /*desc*/)
            -> AltinaEngine::Rhi::FRhiShaderRef override {
            return {};
        }

        auto CreateGraphicsPipeline(const AltinaEngine::Rhi::FRhiGraphicsPipelineDesc& /*desc*/)
            -> AltinaEngine::Rhi::FRhiPipelineRef override {
            return {};
        }
        auto CreateComputePipeline(const AltinaEngine::Rhi::FRhiComputePipelineDesc& /*desc*/)
            -> AltinaEngine::Rhi::FRhiPipelineRef override {
            return {};
        }
        auto CreatePipelineLayout(const AltinaEngine::Rhi::FRhiPipelineLayoutDesc& /*desc*/)
            -> AltinaEngine::Rhi::FRhiPipelineLayoutRef override {
            return {};
        }

        auto CreateBindGroupLayout(const AltinaEngine::Rhi::FRhiBindGroupLayoutDesc& /*desc*/)
            -> AltinaEngine::Rhi::FRhiBindGroupLayoutRef override {
            return {};
        }
        auto CreateBindGroup(const AltinaEngine::Rhi::FRhiBindGroupDesc& /*desc*/)
            -> AltinaEngine::Rhi::FRhiBindGroupRef override {
            return {};
        }

        void UpdateTextureSubresource(AltinaEngine::Rhi::FRhiTexture* /*texture*/,
            const AltinaEngine::Rhi::FRhiTextureSubresource& /*subresource*/, const void* /*data*/,
            u32 /*rowPitchBytes*/, u32 /*slicePitchBytes*/) override {}

        auto CreateFence(u64 initialValue) -> AltinaEngine::Rhi::FRhiFenceRef override {
            return MakeResource<FTestFence>(initialValue);
        }
        auto CreateSemaphore(bool timeline, u64 initialValue)
            -> AltinaEngine::Rhi::FRhiSemaphoreRef override {
            return MakeResource<FTestSemaphore>(timeline, initialValue);
        }
        auto CreateTransition(const AltinaEngine::Rhi::FRhiTransitionDesc& desc)
            -> AltinaEngine::Rhi::FRhiTransitionRef override {
            auto semaphore = CreateSemaphore(true, 0ULL);
            if (!semaphore) {
                return {};
            }
            auto transition = MakeResource<FTestTransition>(desc, Move(semaphore));
            transition->SetSignalValue(1ULL);
            return transition;
        }

        auto CreateCommandPool(const AltinaEngine::Rhi::FRhiCommandPoolDesc& /*desc*/)
            -> AltinaEngine::Rhi::FRhiCommandPoolRef override {
            return {};
        }
        auto CreateCommandList(const AltinaEngine::Rhi::FRhiCommandListDesc& desc)
            -> AltinaEngine::Rhi::FRhiCommandListRef override {
            return MakeResource<FTestCommandList>(desc);
        }
        auto CreateCommandContext(const AltinaEngine::Rhi::FRhiCommandContextDesc& desc)
            -> AltinaEngine::Rhi::FRhiCommandContextRef override {
            auto      queue   = GetQueue(desc.mQueueType);
            auto      context = MakeResource<FTestCommandContext>(desc, queue.Get());
            const u32 index   = static_cast<u32>(desc.mQueueType);
            if (index < 3U) {
                mContexts[index] = context.Get();
            }
            return context;
        }

        FTestQueue* GetTestQueue(AltinaEngine::Rhi::ERhiQueueType type) const {
            auto queue = GetQueue(type);
            return queue ? static_cast<FTestQueue*>(queue.Get()) : nullptr;
        }

        FTestCommandContext* GetTestContext(AltinaEngine::Rhi::ERhiQueueType type) const {
            const u32 index = static_cast<u32>(type);
            return (index < 3U) ? mContexts[index] : nullptr;
        }

    private:
        FTestCommandContext* mContexts[3] = {};
    };
} // namespace

TEST_CASE("FrameGraphExecutor.CrossQueueTransition_SubmitOrder") {
    FTestDevice device(true, true);
    FFrameGraph graph(device);
    graph.BeginFrame(1);

    FRhiTextureDesc texDesc{};
    texDesc.mWidth   = 4U;
    texDesc.mHeight  = 4U;
    texDesc.mFormat  = ERhiFormat::R8G8B8A8Unorm;
    auto externalTex = device.CreateTexture(texDesc);
    REQUIRE(externalTex);
    const auto texRef = graph.ImportTexture(externalTex.Get(), ERhiResourceState::Common);

    struct FPassData {
        FFrameGraphTextureRef mTex;
    };

    FFrameGraphPassDesc passA{};
    passA.mName  = "CrossQueueA";
    passA.mType  = EFrameGraphPassType::Compute;
    passA.mQueue = EFrameGraphQueue::Compute;

    graph.AddPass<FPassData>(
        passA,
        [&](FFrameGraphPassBuilder& builder, FPassData& data) {
            data.mTex = builder.Write(texRef, ERhiResourceState::UnorderedAccess);
        },
        [](AltinaEngine::Rhi::FRhiCmdContext& ctx, const FFrameGraphPassResources& /*res*/,
            const FPassData& /*data*/) { ctx.RHIDispatch(1U, 1U, 1U); });

    FFrameGraphPassDesc passB{};
    passB.mName  = "CrossQueueB";
    passB.mType  = EFrameGraphPassType::Compute;
    passB.mQueue = EFrameGraphQueue::Graphics;

    graph.AddPass<FPassData>(
        passB,
        [&](FFrameGraphPassBuilder& builder, FPassData& data) {
            data.mTex = builder.Read(texRef, ERhiResourceState::ShaderResource);
        },
        [](AltinaEngine::Rhi::FRhiCmdContext& ctx, const FFrameGraphPassResources& /*res*/,
            const FPassData& /*data*/) { ctx.RHIDispatch(1U, 1U, 1U); });

    graph.Compile();
    FFrameGraphExecutor executor(device);
    executor.Execute(graph);

    auto* computeQueue  = device.GetTestQueue(AltinaEngine::Rhi::ERhiQueueType::Compute);
    auto* graphicsQueue = device.GetTestQueue(AltinaEngine::Rhi::ERhiQueueType::Graphics);
    REQUIRE(computeQueue != nullptr);
    REQUIRE(graphicsQueue != nullptr);

    const auto& computeSubmits  = computeQueue->GetSubmits();
    const auto& graphicsSubmits = graphicsQueue->GetSubmits();
    REQUIRE(!computeSubmits.IsEmpty());
    REQUIRE(!graphicsSubmits.IsEmpty());

    bool                              foundSignal = false;
    AltinaEngine::Rhi::FRhiSemaphore* signaled    = nullptr;
    for (const auto& submit : computeSubmits) {
        if (!submit.mSignals.IsEmpty()) {
            foundSignal = true;
            signaled    = submit.mSignals[0].mSemaphore;
            break;
        }
    }
    REQUIRE(foundSignal);
    REQUIRE(signaled != nullptr);

    bool foundWait = false;
    for (const auto& submit : graphicsSubmits) {
        if (!submit.mWaits.IsEmpty()) {
            foundWait = (submit.mWaits[0].mSemaphore == signaled);
            if (foundWait) {
                break;
            }
        }
    }
    REQUIRE(foundWait);

    auto* gfxContext = device.GetTestContext(AltinaEngine::Rhi::ERhiQueueType::Graphics);
    REQUIRE(gfxContext != nullptr);
    const auto& events = gfxContext->GetEvents();
    REQUIRE(!events.IsEmpty());
    bool seenAcquire              = false;
    bool seenDispatchAfterAcquire = false;
    for (const auto evt : events) {
        if (evt == ETestEvent::EndTransition) {
            seenAcquire = true;
        } else if (evt == ETestEvent::Dispatch && seenAcquire) {
            seenDispatchAfterAcquire = true;
            break;
        }
    }
    REQUIRE(seenDispatchAfterAcquire);
}

TEST_CASE("FrameGraphExecutor.QueueFallback_WhenAsyncUnsupported") {
    FTestDevice device(false, false);
    FFrameGraph graph(device);
    graph.BeginFrame(1);

    FFrameGraphPassDesc pass{};
    pass.mName  = "FallbackPass";
    pass.mType  = EFrameGraphPassType::Compute;
    pass.mQueue = EFrameGraphQueue::Compute;

    struct FPassData {};

    graph.AddPass<FPassData>(
        pass,
        [&](FFrameGraphPassBuilder& builder, FPassData& /*data*/) {
            FFrameGraphTextureDesc desc{};
            desc.mDesc.mWidth  = 4U;
            desc.mDesc.mHeight = 4U;
            desc.mDesc.mFormat = ERhiFormat::R8G8B8A8Unorm;
            auto tex           = builder.CreateTexture(desc);
            builder.Read(tex, ERhiResourceState::ShaderResource);
        },
        [](AltinaEngine::Rhi::FRhiCmdContext& ctx, const FFrameGraphPassResources& /*res*/,
            const FPassData& /*data*/) { ctx.RHIDispatch(1U, 1U, 1U); });

    graph.Compile();
    FFrameGraphExecutor executor(device);
    executor.Execute(graph);

    auto* computeQueue  = device.GetTestQueue(AltinaEngine::Rhi::ERhiQueueType::Compute);
    auto* graphicsQueue = device.GetTestQueue(AltinaEngine::Rhi::ERhiQueueType::Graphics);
    REQUIRE(computeQueue != nullptr);
    REQUIRE(graphicsQueue != nullptr);

    REQUIRE(computeQueue->GetSubmits().IsEmpty());
    REQUIRE(!graphicsQueue->GetSubmits().IsEmpty());
}

TEST_CASE("FrameGraph.BasicPassResources") {
    FRhiMockContext context;
    auto            device = CreateMockDevice(context);

    FFrameGraph     graph(*device);
    graph.BeginFrame(1);

    struct FPassData {
        FFrameGraphTextureRef mColor;
        FFrameGraphTextureRef mDepth;
        FFrameGraphBufferRef  mBuffer;
        FFrameGraphSRVRef     mColorSRV;
        FFrameGraphUAVRef     mBufferUAV;
        FFrameGraphRTVRef     mColorRTV;
        FFrameGraphDSVRef     mDepthDSV;
    };

    bool                executed          = false;
    bool                resourcesResolved = false;

    FFrameGraphPassDesc passDesc;
    passDesc.mName  = "FrameGraph.BasicPassResources";
    passDesc.mType  = EFrameGraphPassType::Raster;
    passDesc.mQueue = EFrameGraphQueue::Graphics;

    graph.AddPass<FPassData>(
        passDesc,
        [&](FFrameGraphPassBuilder& builder, FPassData& data) {
            FFrameGraphTextureDesc colorDesc;
            colorDesc.mDesc.mDebugName.Assign(TEXT("FG_Color"));
            colorDesc.mDesc.mWidth  = 4U;
            colorDesc.mDesc.mHeight = 4U;
            colorDesc.mDesc.mFormat = ERhiFormat::R8G8B8A8Unorm;
            colorDesc.mDesc.mBindFlags =
                ERhiTextureBindFlags::RenderTarget | ERhiTextureBindFlags::ShaderResource;

            FFrameGraphTextureDesc depthDesc;
            depthDesc.mDesc.mDebugName.Assign(TEXT("FG_Depth"));
            depthDesc.mDesc.mWidth     = 4U;
            depthDesc.mDesc.mHeight    = 4U;
            depthDesc.mDesc.mFormat    = ERhiFormat::D24UnormS8Uint;
            depthDesc.mDesc.mBindFlags = ERhiTextureBindFlags::DepthStencil;

            FFrameGraphBufferDesc bufferDesc;
            bufferDesc.mDesc.mDebugName.Assign(TEXT("FG_Buffer"));
            bufferDesc.mDesc.mSizeBytes = 256U;
            bufferDesc.mDesc.mBindFlags =
                ERhiBufferBindFlags::ShaderResource | ERhiBufferBindFlags::UnorderedAccess;

            data.mColor  = builder.CreateTexture(colorDesc);
            data.mDepth  = builder.CreateTexture(depthDesc);
            data.mBuffer = builder.CreateBuffer(bufferDesc);

            data.mColor  = builder.Write(data.mColor, ERhiResourceState::RenderTarget);
            data.mDepth  = builder.Write(data.mDepth, ERhiResourceState::DepthWrite);
            data.mBuffer = builder.Write(data.mBuffer, ERhiResourceState::UnorderedAccess);

            FRhiTextureViewRange colorRange{};
            colorRange.mMipCount        = 1U;
            colorRange.mLayerCount      = 1U;
            colorRange.mDepthSliceCount = 1U;

            FRhiShaderResourceViewDesc srvDesc;
            srvDesc.mDebugName.Assign(TEXT("FG_Color_SRV"));
            srvDesc.mFormat       = colorDesc.mDesc.mFormat;
            srvDesc.mTextureRange = colorRange;
            data.mColorSRV        = builder.CreateSRV(data.mColor, srvDesc);

            FRhiBufferViewRange bufferRange{};
            bufferRange.mOffsetBytes = 0ULL;
            bufferRange.mSizeBytes   = bufferDesc.mDesc.mSizeBytes;

            FRhiUnorderedAccessViewDesc uavDesc;
            uavDesc.mDebugName.Assign(TEXT("FG_Buffer_UAV"));
            uavDesc.mBufferRange = bufferRange;
            data.mBufferUAV      = builder.CreateUAV(data.mBuffer, uavDesc);

            FRhiRenderTargetViewDesc rtvDesc;
            rtvDesc.mDebugName.Assign(TEXT("FG_Color_RTV"));
            rtvDesc.mFormat = colorDesc.mDesc.mFormat;
            rtvDesc.mRange  = colorRange;
            data.mColorRTV  = builder.CreateRTV(data.mColor, rtvDesc);

            FRhiDepthStencilViewDesc dsvDesc;
            dsvDesc.mDebugName.Assign(TEXT("FG_Depth_DSV"));
            dsvDesc.mFormat = depthDesc.mDesc.mFormat;
            dsvDesc.mRange  = colorRange;
            data.mDepthDSV  = builder.CreateDSV(data.mDepth, dsvDesc);

            FRdgRenderTargetBinding rtvBinding;
            rtvBinding.mRTV           = data.mColorRTV;
            rtvBinding.mLoadOp        = AltinaEngine::Rhi::ERhiLoadOp::Clear;
            rtvBinding.mClearColor.mR = 0.1f;
            rtvBinding.mClearColor.mG = 0.2f;
            rtvBinding.mClearColor.mB = 0.3f;
            rtvBinding.mClearColor.mA = 1.0f;

            FRdgDepthStencilBinding dsvBinding;
            dsvBinding.mDSV                        = data.mDepthDSV;
            dsvBinding.mDepthLoadOp                = AltinaEngine::Rhi::ERhiLoadOp::Clear;
            dsvBinding.mClearDepthStencil.mDepth   = 1.0f;
            dsvBinding.mClearDepthStencil.mStencil = 0U;

            builder.SetRenderTargets(&rtvBinding, 1U, &dsvBinding);
            builder.SetExternalOutput(data.mColor, ERhiResourceState::Present);
        },
        [&](AltinaEngine::Rhi::FRhiCmdContext&, const FFrameGraphPassResources& res,
            const FPassData& data) {
            executed          = true;
            resourcesResolved = res.GetTexture(data.mColor) != nullptr
                && res.GetTexture(data.mDepth) != nullptr && res.GetBuffer(data.mBuffer) != nullptr
                && res.GetSRV(data.mColorSRV) != nullptr && res.GetUAV(data.mBufferUAV) != nullptr
                && res.GetRTV(data.mColorRTV) != nullptr && res.GetDSV(data.mDepthDSV) != nullptr;
        });

    graph.Compile();

    FTestCmdContext cmdContext;
    graph.Execute(cmdContext);
    graph.EndFrame();

    REQUIRE(executed);
    REQUIRE(resourcesResolved);
}

TEST_CASE("FrameGraph.ImportedTextureRoundTrip") {
    FRhiMockContext context;
    auto            device = CreateMockDevice(context);

    FRhiTextureDesc texDesc;
    texDesc.mDebugName.Assign(TEXT("ImportedTexture"));
    texDesc.mWidth     = 2U;
    texDesc.mHeight    = 2U;
    texDesc.mFormat    = ERhiFormat::R8G8B8A8Unorm;
    texDesc.mBindFlags = ERhiTextureBindFlags::ShaderResource;

    auto externalTexture = device->CreateTexture(texDesc);
    REQUIRE(externalTexture);

    FFrameGraph graph(*device);
    graph.BeginFrame(2);

    bool samePointer = false;
    auto imported = graph.ImportTexture(externalTexture.Get(), ERhiResourceState::ShaderResource);

    FFrameGraphPassDesc passDesc;
    passDesc.mName  = "FrameGraph.ImportedTextureRoundTrip";
    passDesc.mType  = EFrameGraphPassType::Compute;
    passDesc.mQueue = EFrameGraphQueue::Compute;

    graph.AddPass<FFrameGraphTextureRef>(
        passDesc,
        [&](FFrameGraphPassBuilder& builder, FFrameGraphTextureRef& data) {
            data = builder.Read(imported, ERhiResourceState::ShaderResource);
        },
        [&](AltinaEngine::Rhi::FRhiCmdContext&, const FFrameGraphPassResources& res,
            const FFrameGraphTextureRef& data) {
            samePointer = (res.GetTexture(data) == externalTexture.Get());
        });

    graph.Compile();

    FTestCmdContext cmdContext;
    graph.Execute(cmdContext);
    graph.EndFrame();

    REQUIRE(samePointer);
}

TEST_CASE("FrameGraph.ImportedTextureWriteAsRenderTarget") {
    FRhiMockContext context;
    auto            device = CreateMockDevice(context);

    FRhiTextureDesc texDesc;
    texDesc.mDebugName.Assign(TEXT("ImportedTextureRT"));
    texDesc.mWidth     = 4U;
    texDesc.mHeight    = 4U;
    texDesc.mFormat    = ERhiFormat::R8G8B8A8Unorm;
    texDesc.mBindFlags = ERhiTextureBindFlags::RenderTarget | ERhiTextureBindFlags::ShaderResource;

    auto externalTexture = device->CreateTexture(texDesc);
    REQUIRE(externalTexture);

    FFrameGraph graph(*device);
    graph.BeginFrame(3);

    auto imported = graph.ImportTexture(externalTexture.Get(), ERhiResourceState::Common);

    struct FPassData {
        FFrameGraphTextureRef Out;
        FFrameGraphRTVRef     OutRTV;
    };

    bool                executed    = false;
    bool                sameTexture = false;
    bool                hasValidRtv = false;

    FFrameGraphPassDesc passDesc;
    passDesc.mName  = "FrameGraph.ImportedTextureWriteAsRenderTarget";
    passDesc.mType  = EFrameGraphPassType::Raster;
    passDesc.mQueue = EFrameGraphQueue::Graphics;

    graph.AddPass<FPassData>(
        passDesc,
        [&](FFrameGraphPassBuilder& builder, FPassData& data) {
            data.Out = builder.Write(imported, ERhiResourceState::RenderTarget);

            FRhiTextureViewRange range{};
            range.mMipCount        = 1U;
            range.mLayerCount      = 1U;
            range.mDepthSliceCount = 1U;

            FRhiRenderTargetViewDesc rtvDesc{};
            rtvDesc.mDebugName.Assign(TEXT("ImportedTextureRT.RTV"));
            rtvDesc.mFormat = texDesc.mFormat;
            rtvDesc.mRange  = range;
            data.OutRTV     = builder.CreateRTV(data.Out, rtvDesc);

            FRdgRenderTargetBinding rtv{};
            rtv.mRTV    = data.OutRTV;
            rtv.mLoadOp = AltinaEngine::Rhi::ERhiLoadOp::Clear;
            builder.SetRenderTargets(&rtv, 1U, nullptr);

            // This is an external resource; keep pass alive.
            builder.SetSideEffect();
        },
        [&](AltinaEngine::Rhi::FRhiCmdContext&, const FFrameGraphPassResources& res,
            const FPassData& data) {
            executed    = true;
            sameTexture = (res.GetTexture(data.Out) == externalTexture.Get());
            hasValidRtv = (res.GetRTV(data.OutRTV) != nullptr);
        });

    graph.Compile();

    FTestCmdContext cmdContext;
    graph.Execute(cmdContext);
    graph.EndFrame();

    REQUIRE(executed);
    REQUIRE(sameTexture);
    REQUIRE(hasValidRtv);
}
