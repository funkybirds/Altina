#include "FrameGraph/FrameGraph.h"

#include "Rhi/Command/RhiCmdContext.h"
#include "Rhi/RhiDevice.h"
#include "Rhi/RhiResourceView.h"

namespace AltinaEngine::RenderCore {

    auto FFrameGraphPassResources::GetTexture(FFrameGraphTextureRef ref) const
        -> Rhi::FRhiTexture* {
        return mGraph ? mGraph->ResolveTexture(ref) : nullptr;
    }

    auto FFrameGraphPassResources::GetBuffer(FFrameGraphBufferRef ref) const -> Rhi::FRhiBuffer* {
        return mGraph ? mGraph->ResolveBuffer(ref) : nullptr;
    }

    auto FFrameGraphPassResources::GetSRV(FFrameGraphSRVRef ref) const
        -> Rhi::FRhiShaderResourceView* {
        return mGraph ? mGraph->ResolveSRV(ref) : nullptr;
    }

    auto FFrameGraphPassResources::GetUAV(FFrameGraphUAVRef ref) const
        -> Rhi::FRhiUnorderedAccessView* {
        return mGraph ? mGraph->ResolveUAV(ref) : nullptr;
    }

    auto FFrameGraphPassResources::GetRTV(FFrameGraphRTVRef ref) const
        -> Rhi::FRhiRenderTargetView* {
        return mGraph ? mGraph->ResolveRTV(ref) : nullptr;
    }

    auto FFrameGraphPassResources::GetDSV(FFrameGraphDSVRef ref) const
        -> Rhi::FRhiDepthStencilView* {
        return mGraph ? mGraph->ResolveDSV(ref) : nullptr;
    }

    FFrameGraphTextureRef FFrameGraphPassBuilder::CreateTexture(
        const FFrameGraphTextureDesc& desc) {
        return mGraph.CreateTextureInternal(desc);
    }

    FFrameGraphBufferRef FFrameGraphPassBuilder::CreateBuffer(const FFrameGraphBufferDesc& desc) {
        return mGraph.CreateBufferInternal(desc);
    }

    FFrameGraphTextureRef FFrameGraphPassBuilder::Read(
        FFrameGraphTextureRef tex, Rhi::ERhiResourceState state) {
        mGraph.RegisterTextureAccess(mPassIndex, tex, state, false, nullptr);
        return tex;
    }

    FFrameGraphTextureRef FFrameGraphPassBuilder::Write(
        FFrameGraphTextureRef tex, Rhi::ERhiResourceState state) {
        mGraph.RegisterTextureAccess(mPassIndex, tex, state, true, nullptr);
        return tex;
    }

    FFrameGraphBufferRef FFrameGraphPassBuilder::Read(
        FFrameGraphBufferRef buf, Rhi::ERhiResourceState state) {
        mGraph.RegisterBufferAccess(mPassIndex, buf, state, false);
        return buf;
    }

    FFrameGraphBufferRef FFrameGraphPassBuilder::Write(
        FFrameGraphBufferRef buf, Rhi::ERhiResourceState state) {
        mGraph.RegisterBufferAccess(mPassIndex, buf, state, true);
        return buf;
    }

    FFrameGraphTextureRef FFrameGraphPassBuilder::Read(FFrameGraphTextureRef tex,
        Rhi::ERhiResourceState state, const Rhi::FRhiTextureViewRange& range) {
        mGraph.RegisterTextureAccess(mPassIndex, tex, state, false, &range);
        return tex;
    }

    FFrameGraphTextureRef FFrameGraphPassBuilder::Write(FFrameGraphTextureRef tex,
        Rhi::ERhiResourceState state, const Rhi::FRhiTextureViewRange& range) {
        mGraph.RegisterTextureAccess(mPassIndex, tex, state, true, &range);
        return tex;
    }

    FFrameGraphSRVRef FFrameGraphPassBuilder::CreateSRV(
        FFrameGraphTextureRef tex, const Rhi::FRhiShaderResourceViewDesc& desc) {
        return mGraph.CreateSRVInternal(tex, desc);
    }

    FFrameGraphUAVRef FFrameGraphPassBuilder::CreateUAV(
        FFrameGraphTextureRef tex, const Rhi::FRhiUnorderedAccessViewDesc& desc) {
        return mGraph.CreateUAVInternal(tex, desc);
    }

    FFrameGraphSRVRef FFrameGraphPassBuilder::CreateSRV(
        FFrameGraphBufferRef buf, const Rhi::FRhiShaderResourceViewDesc& desc) {
        return mGraph.CreateSRVInternal(buf, desc);
    }

    FFrameGraphUAVRef FFrameGraphPassBuilder::CreateUAV(
        FFrameGraphBufferRef buf, const Rhi::FRhiUnorderedAccessViewDesc& desc) {
        return mGraph.CreateUAVInternal(buf, desc);
    }

    FFrameGraphRTVRef FFrameGraphPassBuilder::CreateRTV(
        FFrameGraphTextureRef tex, const Rhi::FRhiRenderTargetViewDesc& desc) {
        return mGraph.CreateRTVInternal(tex, desc);
    }

    FFrameGraphDSVRef FFrameGraphPassBuilder::CreateDSV(
        FFrameGraphTextureRef tex, const Rhi::FRhiDepthStencilViewDesc& desc) {
        return mGraph.CreateDSVInternal(tex, desc);
    }

    void FFrameGraphPassBuilder::SetRenderTargets(
        const FRdgRenderTargetBinding* RTVs, u32 RTVCount, const FRdgDepthStencilBinding* DSV) {
        mGraph.SetRenderTargetsInternal(mPassIndex, RTVs, RTVCount, DSV);
    }

    void FFrameGraphPassBuilder::SetExternalOutput(
        FFrameGraphTextureRef tex, Rhi::ERhiResourceState finalState) {
        mGraph.SetExternalOutputInternal(mPassIndex, tex, finalState);
    }

    void FFrameGraphPassBuilder::SetSideEffect() { mGraph.SetSideEffectInternal(mPassIndex); }

    FFrameGraph::FFrameGraph(Rhi::FRhiDevice& device) : mDevice(&device) {}

    FFrameGraph::~FFrameGraph() { ResetGraph(); }

    void FFrameGraph::BeginFrame(u64 frameIndex) {
        if (mInFrame) {
            ResetGraph();
        }
        mFrameIndex = frameIndex;
        mInFrame    = true;
        mCompiled   = false;
        if (mDevice) {
            mDevice->BeginFrame(frameIndex);
        }
    }

    void FFrameGraph::EndFrame() {
        ResetGraph();
        if (mDevice) {
            mDevice->EndFrame();
        }
        mInFrame    = false;
        mFrameIndex = 0;
    }

    void FFrameGraph::Compile() {
        if (mCompiled) {
            return;
        }

        if (!mDevice) {
            mCompiled = true;
            return;
        }

        for (auto& texture : mTextures) {
            if (texture.mIsExternal || texture.mTexture) {
                continue;
            }
            texture.mTexture = mDevice->CreateTexture(texture.mDesc.mDesc);
        }

        for (auto& buffer : mBuffers) {
            if (buffer.mIsExternal || buffer.mBuffer) {
                continue;
            }
            buffer.mBuffer = mDevice->CreateBuffer(buffer.mDesc.mDesc);
        }

        for (auto& SRV : mSRVs) {
            auto desc = SRV.mDesc;
            if (SRV.mIsTexture) {
                desc.mTexture = ResolveTexture(FFrameGraphTextureRef{ SRV.mResourceId });
                desc.mBuffer  = nullptr;
            } else {
                desc.mBuffer  = ResolveBuffer(FFrameGraphBufferRef{ SRV.mResourceId });
                desc.mTexture = nullptr;
            }
            SRV.mView = mDevice->CreateShaderResourceView(desc);
        }

        for (auto& UAV : mUAVs) {
            auto desc = UAV.mDesc;
            if (UAV.mIsTexture) {
                desc.mTexture = ResolveTexture(FFrameGraphTextureRef{ UAV.mResourceId });
                desc.mBuffer  = nullptr;
            } else {
                desc.mBuffer  = ResolveBuffer(FFrameGraphBufferRef{ UAV.mResourceId });
                desc.mTexture = nullptr;
            }
            UAV.mView = mDevice->CreateUnorderedAccessView(desc);
        }

        for (auto& RTV : mRTVs) {
            auto desc     = RTV.mDesc;
            desc.mTexture = ResolveTexture(FFrameGraphTextureRef{ RTV.mResourceId });
            RTV.mView     = mDevice->CreateRenderTargetView(desc);
        }

        for (auto& DSV : mDSVs) {
            auto desc     = DSV.mDesc;
            desc.mTexture = ResolveTexture(FFrameGraphTextureRef{ DSV.mResourceId });
            DSV.mView     = mDevice->CreateDepthStencilView(desc);
        }

        for (auto& pass : mPasses) {
            pass.mCompiledColorAttachments.Clear();
            pass.mHasCompiledDepth = false;

            if (!pass.mRenderTargets.IsEmpty()) {
                pass.mCompiledColorAttachments.Resize(pass.mRenderTargets.Size());
                for (usize i = 0; i < pass.mRenderTargets.Size(); ++i) {
                    const auto& binding    = pass.mRenderTargets[i];
                    auto&       attachment = pass.mCompiledColorAttachments[i];
                    attachment.mView       = ResolveRTV(binding.mRTV);
                    attachment.mLoadOp     = binding.mLoadOp;
                    attachment.mStoreOp    = binding.mStoreOp;
                    attachment.mClearColor = binding.mClearColor;
                }
            }

            if (pass.mHasDepthStencil) {
                const auto& binding         = pass.mDepthStencil;
                auto&       depthAtt        = pass.mCompiledDepthAttachment;
                depthAtt.mView              = ResolveDSV(binding.mDSV);
                depthAtt.mDepthLoadOp       = binding.mDepthLoadOp;
                depthAtt.mDepthStoreOp      = binding.mDepthStoreOp;
                depthAtt.mStencilLoadOp     = binding.mStencilLoadOp;
                depthAtt.mStencilStoreOp    = binding.mStencilStoreOp;
                depthAtt.mClearDepthStencil = binding.mClearDepthStencil;
                pass.mHasCompiledDepth      = true;
            }
        }

        mCompiled = true;
    }

    void FFrameGraph::Execute(Rhi::FRhiCmdContext& cmdContext) {
        if (!mCompiled) {
            Compile();
        }

        FFrameGraphPassResources resources(*this);

        for (auto& pass : mPasses) {
            const bool hasRenderPass = pass.mDesc.mType == EFrameGraphPassType::Raster
                && (!pass.mCompiledColorAttachments.IsEmpty() || pass.mHasCompiledDepth);

            if (hasRenderPass) {
                Rhi::FRhiRenderPassDesc renderPassDesc;
                renderPassDesc.mColorAttachmentCount =
                    static_cast<u32>(pass.mCompiledColorAttachments.Size());
                renderPassDesc.mColorAttachments = pass.mCompiledColorAttachments.Data();
                renderPassDesc.mDepthStencilAttachment =
                    pass.mHasCompiledDepth ? &pass.mCompiledDepthAttachment : nullptr;
                cmdContext.RHIBeginRenderPass(renderPassDesc);
            }

            if (pass.mExecute) {
                pass.mExecute(cmdContext, resources, pass.mPassData, pass.mExecuteUserData);
            } else if (pass.mDesc.mExecute) {
                pass.mDesc.mExecute(cmdContext, resources);
            }

            if (hasRenderPass) {
                cmdContext.RHIEndRenderPass();
            }
        }
    }

    FFrameGraphTextureRef FFrameGraph::ImportTexture(
        Rhi::FRhiTexture* external, Rhi::ERhiResourceState state) {
        FRdgTextureEntry entry;
        entry.mIsExternal         = true;
        entry.mExternal           = external;
        entry.mDesc.mInitialState = state;
        mTextures.PushBack(entry);
        mCompiled = false;
        return FFrameGraphTextureRef{ static_cast<u32>(mTextures.Size()) };
    }

    FFrameGraphBufferRef FFrameGraph::ImportBuffer(
        Rhi::FRhiBuffer* external, Rhi::ERhiResourceState state) {
        FRdgBufferEntry entry;
        entry.mIsExternal         = true;
        entry.mExternal           = external;
        entry.mDesc.mInitialState = state;
        mBuffers.PushBack(entry);
        mCompiled = false;
        return FFrameGraphBufferRef{ static_cast<u32>(mBuffers.Size()) };
    }

    auto FFrameGraph::AllocatePass(const FFrameGraphPassDesc& desc) -> u32 {
        auto& pass = mPasses.EmplaceBack();
        pass.mDesc = desc;
        mCompiled  = false;
        return static_cast<u32>(mPasses.Size() - 1U);
    }

    void FFrameGraph::ResetGraph() {
        for (auto& pass : mPasses) {
            if (pass.mDestroyExecute && pass.mExecuteUserData) {
                pass.mDestroyExecute(pass.mExecuteUserData);
            }
            if (pass.mDestroyPassData && pass.mPassData) {
                pass.mDestroyPassData(pass.mPassData);
            }
        }
        mPasses.Clear();
        mTextures.Clear();
        mBuffers.Clear();
        mSRVs.Clear();
        mUAVs.Clear();
        mRTVs.Clear();
        mDSVs.Clear();
        mCompiled = false;
    }

    auto FFrameGraph::CreateTextureInternal(const FFrameGraphTextureDesc& desc)
        -> FFrameGraphTextureRef {
        FRdgTextureEntry entry;
        entry.mDesc = desc;
        mTextures.PushBack(entry);
        mCompiled = false;
        return FFrameGraphTextureRef{ static_cast<u32>(mTextures.Size()) };
    }

    auto FFrameGraph::CreateBufferInternal(const FFrameGraphBufferDesc& desc)
        -> FFrameGraphBufferRef {
        FRdgBufferEntry entry;
        entry.mDesc = desc;
        mBuffers.PushBack(entry);
        mCompiled = false;
        return FFrameGraphBufferRef{ static_cast<u32>(mBuffers.Size()) };
    }

    auto FFrameGraph::CreateSRVInternal(FFrameGraphTextureRef tex,
        const Rhi::FRhiShaderResourceViewDesc&                desc) -> FFrameGraphSRVRef {
        FRdgSRVEntry entry;
        entry.mIsTexture     = true;
        entry.mResourceId    = tex.mId;
        entry.mDesc          = desc;
        entry.mDesc.mTexture = nullptr;
        entry.mDesc.mBuffer  = nullptr;
        mSRVs.PushBack(entry);
        mCompiled = false;
        return FFrameGraphSRVRef{ static_cast<u32>(mSRVs.Size()) };
    }

    auto FFrameGraph::CreateUAVInternal(FFrameGraphTextureRef tex,
        const Rhi::FRhiUnorderedAccessViewDesc&               desc) -> FFrameGraphUAVRef {
        FRdgUAVEntry entry;
        entry.mIsTexture     = true;
        entry.mResourceId    = tex.mId;
        entry.mDesc          = desc;
        entry.mDesc.mTexture = nullptr;
        entry.mDesc.mBuffer  = nullptr;
        mUAVs.PushBack(entry);
        mCompiled = false;
        return FFrameGraphUAVRef{ static_cast<u32>(mUAVs.Size()) };
    }

    auto FFrameGraph::CreateSRVInternal(FFrameGraphBufferRef buf,
        const Rhi::FRhiShaderResourceViewDesc&               desc) -> FFrameGraphSRVRef {
        FRdgSRVEntry entry;
        entry.mIsTexture     = false;
        entry.mResourceId    = buf.mId;
        entry.mDesc          = desc;
        entry.mDesc.mTexture = nullptr;
        entry.mDesc.mBuffer  = nullptr;
        mSRVs.PushBack(entry);
        mCompiled = false;
        return FFrameGraphSRVRef{ static_cast<u32>(mSRVs.Size()) };
    }

    auto FFrameGraph::CreateUAVInternal(FFrameGraphBufferRef buf,
        const Rhi::FRhiUnorderedAccessViewDesc&              desc) -> FFrameGraphUAVRef {
        FRdgUAVEntry entry;
        entry.mIsTexture     = false;
        entry.mResourceId    = buf.mId;
        entry.mDesc          = desc;
        entry.mDesc.mTexture = nullptr;
        entry.mDesc.mBuffer  = nullptr;
        mUAVs.PushBack(entry);
        mCompiled = false;
        return FFrameGraphUAVRef{ static_cast<u32>(mUAVs.Size()) };
    }

    auto FFrameGraph::CreateRTVInternal(
        FFrameGraphTextureRef tex, const Rhi::FRhiRenderTargetViewDesc& desc) -> FFrameGraphRTVRef {
        FRdgRTVEntry entry;
        entry.mResourceId    = tex.mId;
        entry.mDesc          = desc;
        entry.mDesc.mTexture = nullptr;
        mRTVs.PushBack(entry);
        mCompiled = false;
        return FFrameGraphRTVRef{ static_cast<u32>(mRTVs.Size()) };
    }

    auto FFrameGraph::CreateDSVInternal(
        FFrameGraphTextureRef tex, const Rhi::FRhiDepthStencilViewDesc& desc) -> FFrameGraphDSVRef {
        FRdgDSVEntry entry;
        entry.mResourceId    = tex.mId;
        entry.mDesc          = desc;
        entry.mDesc.mTexture = nullptr;
        mDSVs.PushBack(entry);
        mCompiled = false;
        return FFrameGraphDSVRef{ static_cast<u32>(mDSVs.Size()) };
    }

    void FFrameGraph::RegisterTextureAccess(u32 passIndex, FFrameGraphTextureRef tex,
        Rhi::ERhiResourceState state, bool isWrite, const Rhi::FRhiTextureViewRange* range) {
        if (!tex.IsValid() || passIndex >= mPasses.Size()) {
            return;
        }
        const auto index = static_cast<usize>(tex.mId - 1U);
        if (index >= mTextures.Size()) {
            return;
        }

        FRdgResourceAccess access;
        access.mType       = EFrameGraphResourceType::Texture;
        access.mResourceId = tex.mId;
        access.mState      = state;
        access.mIsWrite    = isWrite;
        if (range) {
            access.mHasRange = true;
            access.mRange    = *range;
        }
        mPasses[passIndex].mAccesses.PushBack(access);
    }

    void FFrameGraph::RegisterBufferAccess(
        u32 passIndex, FFrameGraphBufferRef buf, Rhi::ERhiResourceState state, bool isWrite) {
        if (!buf.IsValid() || passIndex >= mPasses.Size()) {
            return;
        }
        const auto index = static_cast<usize>(buf.mId - 1U);
        if (index >= mBuffers.Size()) {
            return;
        }

        FRdgResourceAccess access;
        access.mType       = EFrameGraphResourceType::Buffer;
        access.mResourceId = buf.mId;
        access.mState      = state;
        access.mIsWrite    = isWrite;
        mPasses[passIndex].mAccesses.PushBack(access);
    }

    void FFrameGraph::SetRenderTargetsInternal(u32 passIndex, const FRdgRenderTargetBinding* RTVs,
        u32 RTVCount, const FRdgDepthStencilBinding* DSV) {
        if (passIndex >= mPasses.Size()) {
            return;
        }

        auto& pass = mPasses[passIndex];
        pass.mRenderTargets.Clear();
        if (RTVs && RTVCount > 0U) {
            pass.mRenderTargets.Reserve(RTVCount);
            for (u32 i = 0; i < RTVCount; ++i) {
                pass.mRenderTargets.PushBack(RTVs[i]);
            }
        }
        pass.mHasDepthStencil = (DSV != nullptr);
        if (DSV) {
            pass.mDepthStencil = *DSV;
        }
        mCompiled = false;
    }

    void FFrameGraph::SetExternalOutputInternal(
        u32 passIndex, FFrameGraphTextureRef tex, Rhi::ERhiResourceState finalState) {
        if (passIndex >= mPasses.Size()) {
            return;
        }
        if (tex.IsValid()) {
            const auto index = static_cast<usize>(tex.mId - 1U);
            if (index < mTextures.Size()) {
                auto& entry             = mTextures[index];
                entry.mIsExternalOutput = true;
                entry.mFinalState       = finalState;
            }
        }
        mPasses[passIndex].mDesc.mFlags |= EFrameGraphPassFlags::ExternalOutput;
        mCompiled = false;
    }

    void FFrameGraph::SetSideEffectInternal(u32 passIndex) {
        if (passIndex >= mPasses.Size()) {
            return;
        }
        auto& pass          = mPasses[passIndex];
        pass.mHasSideEffect = true;
        pass.mDesc.mFlags |= EFrameGraphPassFlags::NeverCull;
        mCompiled = false;
    }

    auto FFrameGraph::ResolveTexture(FFrameGraphTextureRef ref) const -> Rhi::FRhiTexture* {
        if (!ref.IsValid()) {
            return nullptr;
        }
        const auto index = static_cast<usize>(ref.mId - 1U);
        if (index >= mTextures.Size()) {
            return nullptr;
        }
        const auto& entry = mTextures[index];
        return entry.mIsExternal ? entry.mExternal : entry.mTexture.Get();
    }

    auto FFrameGraph::ResolveBuffer(FFrameGraphBufferRef ref) const -> Rhi::FRhiBuffer* {
        if (!ref.IsValid()) {
            return nullptr;
        }
        const auto index = static_cast<usize>(ref.mId - 1U);
        if (index >= mBuffers.Size()) {
            return nullptr;
        }
        const auto& entry = mBuffers[index];
        return entry.mIsExternal ? entry.mExternal : entry.mBuffer.Get();
    }

    auto FFrameGraph::ResolveSRV(FFrameGraphSRVRef ref) const -> Rhi::FRhiShaderResourceView* {
        if (!ref.IsValid()) {
            return nullptr;
        }
        const auto index = static_cast<usize>(ref.mId - 1U);
        if (index >= mSRVs.Size()) {
            return nullptr;
        }
        return mSRVs[index].mView.Get();
    }

    auto FFrameGraph::ResolveUAV(FFrameGraphUAVRef ref) const -> Rhi::FRhiUnorderedAccessView* {
        if (!ref.IsValid()) {
            return nullptr;
        }
        const auto index = static_cast<usize>(ref.mId - 1U);
        if (index >= mUAVs.Size()) {
            return nullptr;
        }
        return mUAVs[index].mView.Get();
    }

    auto FFrameGraph::ResolveRTV(FFrameGraphRTVRef ref) const -> Rhi::FRhiRenderTargetView* {
        if (!ref.IsValid()) {
            return nullptr;
        }
        const auto index = static_cast<usize>(ref.mId - 1U);
        if (index >= mRTVs.Size()) {
            return nullptr;
        }
        return mRTVs[index].mView.Get();
    }

    auto FFrameGraph::ResolveDSV(FFrameGraphDSVRef ref) const -> Rhi::FRhiDepthStencilView* {
        if (!ref.IsValid()) {
            return nullptr;
        }
        const auto index = static_cast<usize>(ref.mId - 1U);
        if (index >= mDSVs.Size()) {
            return nullptr;
        }
        return mDSVs[index].mView.Get();
    }

} // namespace AltinaEngine::RenderCore
