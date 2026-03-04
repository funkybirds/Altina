#include "FrameGraph/FrameGraph.h"

#include "Rhi/Command/RhiCmdContext.h"
#include "Rhi/RhiDevice.h"
#include "Rhi/RhiResourceView.h"
#include "Logging/Log.h"
#include "Utility/String/CodeConvert.h"
#include "Utility/Assert.h"

#include <cstring>
#include <string>

namespace AltinaEngine::RenderCore {
    using Core::Utility::DebugAssert;

    namespace {
        struct FCompiledResourceState {
            bool                   mInitialized = false;
            Rhi::ERhiResourceState mState       = Rhi::ERhiResourceState::Unknown;
        };
    } // namespace

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
            DebugAssert(static_cast<bool>(texture.mTexture), TEXT("RenderCore.FrameGraph"),
                "CreateTexture failed: debugName='{}', width={}, height={}, format={}, bindFlags={}.",
                texture.mDesc.mDesc.mDebugName.ToView(), texture.mDesc.mDesc.mWidth,
                texture.mDesc.mDesc.mHeight, static_cast<u32>(texture.mDesc.mDesc.mFormat),
                static_cast<u32>(texture.mDesc.mDesc.mBindFlags));
        }

        for (auto& buffer : mBuffers) {
            if (buffer.mIsExternal || buffer.mBuffer) {
                continue;
            }
            buffer.mBuffer = mDevice->CreateBuffer(buffer.mDesc.mDesc);
            DebugAssert(static_cast<bool>(buffer.mBuffer), TEXT("RenderCore.FrameGraph"),
                "CreateBuffer failed: debugName='{}', sizeBytes={}, usage={}, bindFlags={}, cpuAccess={}.",
                buffer.mDesc.mDesc.mDebugName.ToView(), buffer.mDesc.mDesc.mSizeBytes,
                static_cast<u32>(buffer.mDesc.mDesc.mUsage),
                static_cast<u32>(buffer.mDesc.mDesc.mBindFlags),
                static_cast<u32>(buffer.mDesc.mDesc.mCpuAccess));
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
            DebugAssert((desc.mTexture != nullptr) || (desc.mBuffer != nullptr),
                TEXT("RenderCore.FrameGraph"),
                "CreateSRV failed precondition: resource is null (isTexture={}, resourceId={}).",
                SRV.mIsTexture ? 1U : 0U, SRV.mResourceId);
            SRV.mView = mDevice->CreateShaderResourceView(desc);
            DebugAssert(static_cast<bool>(SRV.mView), TEXT("RenderCore.FrameGraph"),
                "CreateSRV failed: isTexture={}, resourceId={}, debugName='{}'.",
                SRV.mIsTexture ? 1U : 0U, SRV.mResourceId, desc.mDebugName.ToView());
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
            DebugAssert((desc.mTexture != nullptr) || (desc.mBuffer != nullptr),
                TEXT("RenderCore.FrameGraph"),
                "CreateUAV failed precondition: resource is null (isTexture={}, resourceId={}).",
                UAV.mIsTexture ? 1U : 0U, UAV.mResourceId);
            UAV.mView = mDevice->CreateUnorderedAccessView(desc);
            DebugAssert(static_cast<bool>(UAV.mView), TEXT("RenderCore.FrameGraph"),
                "CreateUAV failed: isTexture={}, resourceId={}, debugName='{}'.",
                UAV.mIsTexture ? 1U : 0U, UAV.mResourceId, desc.mDebugName.ToView());
        }

        for (auto& RTV : mRTVs) {
            auto desc     = RTV.mDesc;
            desc.mTexture = ResolveTexture(FFrameGraphTextureRef{ RTV.mResourceId });
            DebugAssert(desc.mTexture != nullptr, TEXT("RenderCore.FrameGraph"),
                "CreateRTV failed precondition: texture is null (resourceId={}, debugName='{}').",
                RTV.mResourceId, desc.mDebugName.ToView());
            RTV.mView = mDevice->CreateRenderTargetView(desc);
            DebugAssert(static_cast<bool>(RTV.mView), TEXT("RenderCore.FrameGraph"),
                "CreateRTV failed: resourceId={}, debugName='{}', format={}.", RTV.mResourceId,
                desc.mDebugName.ToView(), static_cast<u32>(desc.mFormat));
        }

        for (auto& DSV : mDSVs) {
            auto desc     = DSV.mDesc;
            desc.mTexture = ResolveTexture(FFrameGraphTextureRef{ DSV.mResourceId });
            DebugAssert(desc.mTexture != nullptr, TEXT("RenderCore.FrameGraph"),
                "CreateDSV failed precondition: texture is null (resourceId={}, debugName='{}').",
                DSV.mResourceId, desc.mDebugName.ToView());
            DSV.mView = mDevice->CreateDepthStencilView(desc);
            DebugAssert(static_cast<bool>(DSV.mView), TEXT("RenderCore.FrameGraph"),
                "CreateDSV failed: resourceId={}, debugName='{}', format={}.", DSV.mResourceId,
                desc.mDebugName.ToView(), static_cast<u32>(desc.mFormat));
        }

        mCompiledBeginTransitions.Clear();
        mCompiledFinalTransitions.Clear();
        TVector<FCompiledResourceState> textureStates;
        TVector<FCompiledResourceState> bufferStates;
        TVector<bool>                   externalBeginTransitionEmitted;
        textureStates.Resize(mTextures.Size());
        bufferStates.Resize(mBuffers.Size());
        externalBeginTransitionEmitted.Resize(mTextures.Size());
        for (usize i = 0; i < mTextures.Size(); ++i) {
            textureStates[i].mInitialized     = true;
            textureStates[i].mState           = mTextures[i].mDesc.mInitialState;
            externalBeginTransitionEmitted[i] = false;
        }
        for (usize i = 0; i < mBuffers.Size(); ++i) {
            bufferStates[i].mInitialized = true;
            bufferStates[i].mState       = mBuffers[i].mDesc.mInitialState;
        }

        for (auto& pass : mPasses) {
            pass.mCompiledColorAttachments.Clear();
            pass.mHasCompiledDepth = false;
            pass.mCompiledPreTransitions.Clear();

            if (!pass.mRenderTargets.IsEmpty()) {
                pass.mCompiledColorAttachments.Reserve(pass.mRenderTargets.Size());
                for (usize i = 0; i < pass.mRenderTargets.Size(); ++i) {
                    const auto& binding = pass.mRenderTargets[i];
                    auto*       view    = ResolveRTV(binding.mRTV);
                    if (view == nullptr) {
                        continue;
                    }
                    auto& attachment       = pass.mCompiledColorAttachments.EmplaceBack();
                    attachment.mView       = view;
                    attachment.mLoadOp     = binding.mLoadOp;
                    attachment.mStoreOp    = binding.mStoreOp;
                    attachment.mClearColor = binding.mClearColor;
                }
            }

            pass.mHasCompiledDepth = false;
            if (pass.mHasDepthStencil) {
                const auto& binding         = pass.mDepthStencil;
                auto&       depthAtt        = pass.mCompiledDepthAttachment;
                depthAtt.mView              = ResolveDSV(binding.mDSV);
                depthAtt.mDepthLoadOp       = binding.mDepthLoadOp;
                depthAtt.mDepthStoreOp      = binding.mDepthStoreOp;
                depthAtt.mStencilLoadOp     = binding.mStencilLoadOp;
                depthAtt.mStencilStoreOp    = binding.mStencilStoreOp;
                depthAtt.mClearDepthStencil = binding.mClearDepthStencil;
                pass.mHasCompiledDepth      = (depthAtt.mView != nullptr);
            }

            for (const auto& access : pass.mAccesses) {
                if (access.mType == EFrameGraphResourceType::Texture) {
                    if (access.mResourceId == 0U) {
                        continue;
                    }
                    const usize texIndex = static_cast<usize>(access.mResourceId - 1U);
                    if (texIndex >= textureStates.Size()) {
                        continue;
                    }
                    auto& state = textureStates[texIndex];
                    if (!state.mInitialized) {
                        state.mInitialized = true;
                        state.mState       = access.mState;
                        continue;
                    }
                    if (state.mState == access.mState) {
                        continue;
                    }
                    auto* texture = ResolveTexture(FFrameGraphTextureRef{ access.mResourceId });
                    if (texture == nullptr) {
                        continue;
                    }
                    Rhi::FRhiTransitionInfo info{};
                    info.mResource = texture;
                    info.mBefore   = state.mState;
                    info.mAfter    = access.mState;
                    if (access.mHasRange) {
                        info.mTextureRange = access.mRange;
                    }
                    const bool isFirstExternalTransition = mTextures[texIndex].mIsExternal
                        && !externalBeginTransitionEmitted[texIndex];
                    if (isFirstExternalTransition) {
                        mCompiledBeginTransitions.PushBack(info);
                        externalBeginTransitionEmitted[texIndex] = true;
                    } else {
                        pass.mCompiledPreTransitions.PushBack(info);
                    }
                    state.mState = access.mState;
                    continue;
                }

                if (access.mResourceId == 0U) {
                    continue;
                }
                const usize bufIndex = static_cast<usize>(access.mResourceId - 1U);
                if (bufIndex >= bufferStates.Size()) {
                    continue;
                }
                auto& state = bufferStates[bufIndex];
                if (!state.mInitialized) {
                    state.mInitialized = true;
                    state.mState       = access.mState;
                    continue;
                }
                if (state.mState == access.mState) {
                    continue;
                }
                auto* buffer = ResolveBuffer(FFrameGraphBufferRef{ access.mResourceId });
                if (buffer == nullptr) {
                    continue;
                }
                Rhi::FRhiTransitionInfo info{};
                info.mResource = buffer;
                info.mBefore   = state.mState;
                info.mAfter    = access.mState;
                pass.mCompiledPreTransitions.PushBack(info);
                state.mState = access.mState;
            }
        }

        for (usize i = 0; i < mTextures.Size(); ++i) {
            const auto& entry = mTextures[i];
            if (!entry.mIsExternalOutput || entry.mFinalState == Rhi::ERhiResourceState::Unknown) {
                continue;
            }
            if (i >= textureStates.Size()) {
                continue;
            }
            const auto& state = textureStates[i];
            if (!state.mInitialized || state.mState == entry.mFinalState) {
                continue;
            }
            auto* texture = ResolveTexture(FFrameGraphTextureRef{ static_cast<u32>(i + 1U) });
            if (texture == nullptr) {
                continue;
            }
            Rhi::FRhiTransitionInfo info{};
            info.mResource = texture;
            info.mBefore   = state.mState;
            info.mAfter    = entry.mFinalState;
            mCompiledFinalTransitions.PushBack(info);
        }

        mCompiled = true;
    }

    void FFrameGraph::Execute(Rhi::FRhiCmdContext& cmdContext) {
        if (!mCompiled) {
            Compile();
        }

        FFrameGraphPassResources resources(*this);

        static bool              sLoggedOnce = false;
        if (!sLoggedOnce) {
            sLoggedOnce   = true;
            u32 passIndex = 0U;
            for (const auto& pass : mPasses) {
                const std::string name =
                    (pass.mDesc.mName != nullptr) ? std::string(pass.mDesc.mName) : "<null>";
                Core::Logging::LogInfo(TEXT("FG Pass[{}]: {} type={} rtvs={} depth={}"), passIndex,
                    name.c_str(), static_cast<u32>(pass.mDesc.mType),
                    static_cast<u32>(pass.mCompiledColorAttachments.Size()),
                    pass.mHasCompiledDepth ? 1U : 0U);
                ++passIndex;
            }
        }
        if (!mCompiledBeginTransitions.IsEmpty()) {
            Rhi::FRhiTransitionCreateInfo transition{};
            transition.mTransitions     = mCompiledBeginTransitions.Data();
            transition.mTransitionCount = static_cast<u32>(mCompiledBeginTransitions.Size());
            transition.mSrcQueue        = Rhi::ERhiQueueType::Graphics;
            transition.mDstQueue        = Rhi::ERhiQueueType::Graphics;
            cmdContext.RHIBeginTransition(transition);
        }

        for (auto& pass : mPasses) {
            if (!pass.mCompiledPreTransitions.IsEmpty()) {
                Rhi::FRhiTransitionCreateInfo transition{};
                transition.mTransitions     = pass.mCompiledPreTransitions.Data();
                transition.mTransitionCount = static_cast<u32>(pass.mCompiledPreTransitions.Size());
                transition.mSrcQueue        = Rhi::ERhiQueueType::Graphics;
                transition.mDstQueue        = Rhi::ERhiQueueType::Graphics;
                cmdContext.RHIBeginTransition(transition);
            }

            const bool hasRenderPass = pass.mDesc.mType == EFrameGraphPassType::Raster
                && (!pass.mCompiledColorAttachments.IsEmpty() || pass.mHasCompiledDepth);
            if (pass.mDesc.mType == EFrameGraphPassType::Raster && !hasRenderPass) {
                continue;
            }

            if (hasRenderPass) {
                Rhi::FRhiRenderPassDesc renderPassDesc;
                if (pass.mDesc.mName != nullptr) {
                    renderPassDesc.mDebugName.Assign(Core::Utility::String::FromUtf8Bytes(
                        pass.mDesc.mName, static_cast<usize>(std::strlen(pass.mDesc.mName))));
                }
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

        if (!mCompiledFinalTransitions.IsEmpty()) {
            Rhi::FRhiTransitionCreateInfo transition{};
            transition.mTransitions     = mCompiledFinalTransitions.Data();
            transition.mTransitionCount = static_cast<u32>(mCompiledFinalTransitions.Size());
            transition.mSrcQueue        = Rhi::ERhiQueueType::Graphics;
            transition.mDstQueue        = Rhi::ERhiQueueType::Graphics;
            cmdContext.RHIBeginTransition(transition);
        }
    }

    FFrameGraphTextureRef FFrameGraph::ImportTexture(
        Rhi::FRhiTexture* external, Rhi::ERhiResourceState state) {
        FRdgTextureEntry entry;
        entry.mIsExternal         = true;
        entry.mExternal           = external;
        entry.mDesc.mInitialState = state;
        if (state == Rhi::ERhiResourceState::Present) {
            // Treat imported presentable textures (swapchain backbuffer) as implicit external
            // outputs so FrameGraph always transitions them back to Present at frame end.
            entry.mIsExternalOutput = true;
            entry.mFinalState       = Rhi::ERhiResourceState::Present;
        }
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
        mCompiledBeginTransitions.Clear();
        mCompiledFinalTransitions.Clear();
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
                const auto rtvIndex = static_cast<usize>(RTVs[i].mRTV.mId - 1U);
                if (RTVs[i].mRTV.IsValid() && rtvIndex < mRTVs.Size()) {
                    FRdgResourceAccess access{};
                    access.mType       = EFrameGraphResourceType::Texture;
                    access.mResourceId = mRTVs[rtvIndex].mResourceId;
                    access.mState      = Rhi::ERhiResourceState::RenderTarget;
                    access.mIsWrite    = true;
                    pass.mAccesses.PushBack(access);
                }
            }
        }
        pass.mHasDepthStencil = (DSV != nullptr);
        if (DSV) {
            pass.mDepthStencil  = *DSV;
            const auto dsvIndex = static_cast<usize>(DSV->mDSV.mId - 1U);
            if (DSV->mDSV.IsValid() && dsvIndex < mDSVs.Size()) {
                FRdgResourceAccess access{};
                access.mType       = EFrameGraphResourceType::Texture;
                access.mResourceId = mDSVs[dsvIndex].mResourceId;
                access.mState      = Rhi::ERhiResourceState::DepthWrite;
                access.mIsWrite    = true;
                pass.mAccesses.PushBack(access);
            }
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
