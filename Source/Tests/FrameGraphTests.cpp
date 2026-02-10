#include "TestHarness.h"

#include "FrameGraph/FrameGraph.h"
#include "RhiMock/RhiMockContext.h"
#include "Rhi/Command/RhiCmdContext.h"
#include "Rhi/RhiDevice.h"
#include "Rhi/RhiTexture.h"
#include "Rhi/RhiStructs.h"

namespace {
    using AltinaEngine::TChar;
    using AltinaEngine::i32;
    using AltinaEngine::u32;
    using AltinaEngine::u64;
    using AltinaEngine::RenderCore::EFrameGraphPassType;
    using AltinaEngine::RenderCore::EFrameGraphQueue;
    using AltinaEngine::RenderCore::FFrameGraph;
    using AltinaEngine::RenderCore::FFrameGraphBufferDesc;
    using AltinaEngine::RenderCore::FFrameGraphBufferRef;
    using AltinaEngine::RenderCore::FFrameGraphDSVRef;
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
    using AltinaEngine::Rhi::FRhiInitDesc;
    using AltinaEngine::Rhi::FRhiShaderResourceViewDesc;
    using AltinaEngine::Rhi::FRhiTextureDesc;
    using AltinaEngine::Rhi::FRhiUnorderedAccessViewDesc;
    using AltinaEngine::Rhi::FRhiRenderTargetViewDesc;
    using AltinaEngine::Rhi::FRhiDepthStencilViewDesc;
    using AltinaEngine::Rhi::FRhiTextureViewRange;
    using AltinaEngine::Rhi::FRhiBufferViewRange;
    using AltinaEngine::Rhi::FRhiMockContext;

    auto MakeAdapterDesc(const TChar* name, ERhiAdapterType type, ERhiVendorId vendor) -> FRhiAdapterDesc {
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
        void Begin() override {}
        void End() override {}

        void RHISetGraphicsPipeline(AltinaEngine::Rhi::FRhiPipeline* /*pipeline*/) override {}
        void RHISetComputePipeline(AltinaEngine::Rhi::FRhiPipeline* /*pipeline*/) override {}
        void RHISetPrimitiveTopology(AltinaEngine::Rhi::ERhiPrimitiveTopology /*topology*/) override {}
        void RHISetVertexBuffer(u32 /*slot*/, const AltinaEngine::Rhi::FRhiVertexBufferView& /*view*/) override {}
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
} // namespace

TEST_CASE("FrameGraph.BasicPassResources") {
    FRhiMockContext context;
    auto device = CreateMockDevice(context);

    FFrameGraph graph(*device);
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

    bool executed = false;
    bool resourcesResolved = false;

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
            depthDesc.mDesc.mWidth  = 4U;
            depthDesc.mDesc.mHeight = 4U;
            depthDesc.mDesc.mFormat = ERhiFormat::D24UnormS8Uint;
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
            data.mColorSRV = builder.CreateSRV(data.mColor, srvDesc);

            FRhiBufferViewRange bufferRange{};
            bufferRange.mOffsetBytes = 0ULL;
            bufferRange.mSizeBytes   = bufferDesc.mDesc.mSizeBytes;

            FRhiUnorderedAccessViewDesc uavDesc;
            uavDesc.mDebugName.Assign(TEXT("FG_Buffer_UAV"));
            uavDesc.mBufferRange = bufferRange;
            data.mBufferUAV = builder.CreateUAV(data.mBuffer, uavDesc);

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
            rtvBinding.mRTV = data.mColorRTV;
            rtvBinding.mLoadOp = AltinaEngine::Rhi::ERhiLoadOp::Clear;
            rtvBinding.mClearColor.mR = 0.1f;
            rtvBinding.mClearColor.mG = 0.2f;
            rtvBinding.mClearColor.mB = 0.3f;
            rtvBinding.mClearColor.mA = 1.0f;

            FRdgDepthStencilBinding dsvBinding;
            dsvBinding.mDSV = data.mDepthDSV;
            dsvBinding.mDepthLoadOp = AltinaEngine::Rhi::ERhiLoadOp::Clear;
            dsvBinding.mClearDepthStencil.mDepth = 1.0f;
            dsvBinding.mClearDepthStencil.mStencil = 0U;

            builder.SetRenderTargets(&rtvBinding, 1U, &dsvBinding);
            builder.SetExternalOutput(data.mColor, ERhiResourceState::Present);
        },
        [&](AltinaEngine::Rhi::FRhiCmdContext&, const FFrameGraphPassResources& res,
            const FPassData& data) {
            executed = true;
            resourcesResolved =
                res.GetTexture(data.mColor) != nullptr
                && res.GetTexture(data.mDepth) != nullptr
                && res.GetBuffer(data.mBuffer) != nullptr
                && res.GetSRV(data.mColorSRV) != nullptr
                && res.GetUAV(data.mBufferUAV) != nullptr
                && res.GetRTV(data.mColorRTV) != nullptr
                && res.GetDSV(data.mDepthDSV) != nullptr;
        });

    graph.Compile();

    FTestCmdContext cmdContext;
    cmdContext.Begin();
    graph.Execute(cmdContext);
    cmdContext.End();
    graph.EndFrame();

    REQUIRE(executed);
    REQUIRE(resourcesResolved);
}

TEST_CASE("FrameGraph.ImportedTextureRoundTrip") {
    FRhiMockContext context;
    auto device = CreateMockDevice(context);

    FRhiTextureDesc texDesc;
    texDesc.mDebugName.Assign(TEXT("ImportedTexture"));
    texDesc.mWidth  = 2U;
    texDesc.mHeight = 2U;
    texDesc.mFormat = ERhiFormat::R8G8B8A8Unorm;
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
    cmdContext.Begin();
    graph.Execute(cmdContext);
    cmdContext.End();
    graph.EndFrame();

    REQUIRE(samePointer);
}
