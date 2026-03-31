#include "FrameGraph/FrameGraphExecutor.h"

#include "Rhi/Command/RhiCmdContextAdapter.h"
#include "Rhi/RhiBuffer.h"
#include "Rhi/RhiCommandContext.h"
#include "Rhi/RhiDebugMarker.h"
#include "Rhi/RhiQueue.h"
#include "Rhi/RhiTexture.h"
#include "Rhi/RhiTransition.h"
#include "Logging/Log.h"
#include "Utility/Assert.h"
#include "Utility/String/CodeConvert.h"

#include <chrono>
#include <cstring>

namespace AltinaEngine::RenderCore {
    namespace {
        constexpr auto kFrameTimingCategory = TEXT("FrameTiming");
        using AltinaEngine::u32;
        using AltinaEngine::u64;
        using AltinaEngine::usize;
        using AltinaEngine::Rhi::ERhiQueueType;
        using AltinaEngine::Rhi::ERhiResourceState;
        using AltinaEngine::Rhi::FRhiCmdContextAdapter;
        using AltinaEngine::Rhi::FRhiCommandContext;
        using AltinaEngine::Rhi::FRhiCommandContextDesc;
        using AltinaEngine::Rhi::FRhiDebugMarker;
        using AltinaEngine::Rhi::FRhiDevice;
        using AltinaEngine::Rhi::FRhiQueue;
        using AltinaEngine::Rhi::FRhiQueueSignal;
        using AltinaEngine::Rhi::FRhiQueueWait;
        using AltinaEngine::Rhi::FRhiTransition;
        using AltinaEngine::Rhi::FRhiTransitionCreateInfo;
        using AltinaEngine::Rhi::FRhiTransitionInfo;
        using AltinaEngine::Rhi::IRhiCmdContextOps;

        struct FQueueState {
            ERhiQueueType                            mType = ERhiQueueType::Graphics;
            AltinaEngine::Rhi::FRhiQueueRef          mQueue;
            AltinaEngine::Rhi::FRhiCommandContextRef mContext;
            IRhiCmdContextOps*                       mOps         = nullptr;
            bool                                     mRecording   = false;
            bool                                     mHasCommands = false;
            Core::Container::TVector<FRhiQueueWait>  mPendingWaits;
        };

        struct FTransitionEdge {
            u32                                          mSrcPass  = 0U;
            u32                                          mDstPass  = 0U;
            ERhiQueueType                                mSrcQueue = ERhiQueueType::Graphics;
            ERhiQueueType                                mDstQueue = ERhiQueueType::Graphics;
            Core::Container::TVector<FRhiTransitionInfo> mInfos;
            AltinaEngine::Rhi::FRhiTransitionRef         mTransition;
            FRhiTransitionCreateInfo                     mCreateInfo{};
            bool                                         mCrossQueue = false;
        };

        struct FPassTransitions {
            Core::Container::TVector<u32> mSameQueueEdges;
            Core::Container::TVector<u32> mAcquireEdges;
            Core::Container::TVector<u32> mReleaseEdges;
        };

        struct FResourceState {
            bool              mInitialized = false;
            ERhiResourceState mState       = ERhiResourceState::Unknown;
            ERhiQueueType     mQueue       = ERhiQueueType::Graphics;
            u32               mLastPass    = 0U;
        };

        [[nodiscard]] auto ToRhiQueue(EFrameGraphQueue queue,
            const Rhi::FRhiQueueCapabilities&          caps) noexcept -> ERhiQueueType {
            switch (queue) {
                case EFrameGraphQueue::Compute:
                    return caps.mSupportsAsyncCompute ? ERhiQueueType::Compute
                                                      : ERhiQueueType::Graphics;
                case EFrameGraphQueue::Copy:
                    return caps.mSupportsAsyncCopy ? ERhiQueueType::Copy : ERhiQueueType::Graphics;
                case EFrameGraphQueue::Graphics:
                default:
                    return ERhiQueueType::Graphics;
            }
        }

        [[nodiscard]] auto GetQueueIndex(ERhiQueueType type) noexcept -> u32 {
            return static_cast<u32>(type);
        }

        void BeginQueueIfNeeded(FQueueState& state) {
            if (state.mRecording || !state.mContext || state.mOps == nullptr) {
                return;
            }
            state.mRecording   = true;
            state.mHasCommands = true;
        }

        void SubmitQueue(
            FQueueState& state, const Core::Container::TVector<FRhiQueueSignal>* signals) {
            if (!state.mRecording) {
                return;
            }
            if (!state.mContext) {
                state.mPendingWaits.Clear();
                state.mRecording   = false;
                state.mHasCommands = false;
                return;
            }

            Rhi::FRhiCommandContextSubmitInfo submit{};
            if (!state.mPendingWaits.IsEmpty()) {
                submit.mWaits     = state.mPendingWaits.Data();
                submit.mWaitCount = static_cast<u32>(state.mPendingWaits.Size());
            }
            if (signals && !signals->IsEmpty()) {
                submit.mSignals     = signals->Data();
                submit.mSignalCount = static_cast<u32>(signals->Size());
            }
            state.mContext->RHIFlushContextDevice(submit);

            state.mPendingWaits.Clear();
            state.mRecording   = false;
            state.mHasCommands = false;
        }

        void               SubmitQueue(FQueueState& state) { SubmitQueue(state, nullptr); }

        [[nodiscard]] auto ElapsedMilliseconds(
            const std::chrono::steady_clock::time_point& startTime) noexcept -> double {
            using namespace std::chrono;
            return duration_cast<duration<double, std::milli>>(steady_clock::now() - startTime)
                .count();
        }
    } // namespace

    FFrameGraphExecutor::FFrameGraphExecutor(Rhi::FRhiDevice& device) : mDevice(&device) {}

    void FFrameGraphExecutor::Execute(FFrameGraph& graph) {
        if (mDevice == nullptr) {
            return;
        }
        if (!graph.mCompiled) {
            graph.Compile();
        }

        const auto&                             caps = mDevice->GetQueueCapabilities();

        Core::Container::TVector<ERhiQueueType> passQueues;
        passQueues.Resize(graph.mPasses.Size());
        for (usize i = 0; i < graph.mPasses.Size(); ++i) {
            passQueues[i] = ToRhiQueue(graph.mPasses[i].mDesc.mQueue, caps);
        }

        FQueueState queues[3]{};
        for (u32 i = 0U; i < 3U; ++i) {
            ERhiQueueType type = static_cast<ERhiQueueType>(i);
            queues[i].mType    = type;
            queues[i].mQueue   = mDevice->GetQueue(type);
            if (!queues[i].mQueue) {
                continue;
            }
            FRhiCommandContextDesc desc{};
            desc.mQueueType = type;
            auto context    = mDevice->CreateCommandContext(desc);
            if (!context) {
                continue;
            }
            auto* ops = dynamic_cast<IRhiCmdContextOps*>(context.Get());
            Core::Utility::Assert(ops != nullptr, TEXT("RenderCore"),
                "Command context does not implement IRhiCmdContextOps.");
            queues[i].mContext = Move(context);
            queues[i].mOps     = ops;
        }

        Core::Container::TVector<FTransitionEdge>  edges;
        Core::Container::TVector<FPassTransitions> passTransitions;
        passTransitions.Resize(graph.mPasses.Size());

        Core::Container::TVector<FResourceState> textureStates;
        Core::Container::TVector<FResourceState> bufferStates;
        textureStates.Resize(graph.mTextures.Size());
        bufferStates.Resize(graph.mBuffers.Size());

        for (usize i = 0; i < graph.mTextures.Size(); ++i) {
            textureStates[i].mState = graph.mTextures[i].mDesc.mInitialState;
        }
        for (usize i = 0; i < graph.mBuffers.Size(); ++i) {
            bufferStates[i].mState = graph.mBuffers[i].mDesc.mInitialState;
        }

        for (u32 passIndex = 0U; passIndex < static_cast<u32>(graph.mPasses.Size()); ++passIndex) {
            const auto& pass  = graph.mPasses[passIndex];
            const auto  queue = passQueues[passIndex];

            for (const auto& access : pass.mAccesses) {
                if (access.mType == FFrameGraph::EFrameGraphResourceType::Texture) {
                    const auto texIndex = static_cast<usize>(access.mResourceId - 1U);
                    if (texIndex >= textureStates.Size()) {
                        continue;
                    }
                    auto& state = textureStates[texIndex];
                    auto* texture =
                        graph.ResolveTexture(FFrameGraphTextureRef{ access.mResourceId });
                    if (texture == nullptr) {
                        continue;
                    }

                    if (!state.mInitialized) {
                        if (state.mState != access.mState) {
                            FTransitionEdge edge{};
                            edge.mSrcPass    = passIndex;
                            edge.mDstPass    = passIndex;
                            edge.mSrcQueue   = queue;
                            edge.mDstQueue   = queue;
                            edge.mCrossQueue = false;

                            FRhiTransitionInfo info{};
                            info.mResource = texture;
                            info.mBefore   = state.mState;
                            info.mAfter    = access.mState;
                            if (access.mHasRange) {
                                info.mTextureRange = access.mRange;
                            }
                            edge.mInfos.PushBack(info);
                            edges.PushBack(edge);
                            passTransitions[passIndex].mSameQueueEdges.PushBack(
                                static_cast<u32>(edges.Size() - 1U));
                        }
                        state.mInitialized = true;
                        state.mState       = access.mState;
                        state.mQueue       = queue;
                        state.mLastPass    = passIndex;
                        continue;
                    }

                    if (state.mState == access.mState && state.mQueue == queue) {
                        state.mLastPass = passIndex;
                        continue;
                    }

                    FTransitionEdge edge{};
                    edge.mSrcPass    = state.mLastPass;
                    edge.mDstPass    = passIndex;
                    edge.mSrcQueue   = state.mQueue;
                    edge.mDstQueue   = queue;
                    edge.mCrossQueue = (state.mQueue != queue);

                    FRhiTransitionInfo info{};
                    info.mResource = texture;
                    info.mBefore   = state.mState;
                    info.mAfter    = access.mState;
                    if (access.mHasRange) {
                        info.mTextureRange = access.mRange;
                    }
                    edge.mInfos.PushBack(info);
                    edges.PushBack(edge);
                    const u32 edgeIndex = static_cast<u32>(edges.Size() - 1U);
                    if (edge.mCrossQueue) {
                        passTransitions[edge.mSrcPass].mReleaseEdges.PushBack(edgeIndex);
                        passTransitions[edge.mDstPass].mAcquireEdges.PushBack(edgeIndex);
                    } else {
                        passTransitions[edge.mDstPass].mSameQueueEdges.PushBack(edgeIndex);
                    }

                    state.mState    = access.mState;
                    state.mQueue    = queue;
                    state.mLastPass = passIndex;
                } else {
                    const auto bufIndex = static_cast<usize>(access.mResourceId - 1U);
                    if (bufIndex >= bufferStates.Size()) {
                        continue;
                    }
                    auto& state  = bufferStates[bufIndex];
                    auto* buffer = graph.ResolveBuffer(FFrameGraphBufferRef{ access.mResourceId });
                    if (buffer == nullptr) {
                        continue;
                    }

                    if (!state.mInitialized) {
                        if (state.mState != access.mState) {
                            FTransitionEdge edge{};
                            edge.mSrcPass    = passIndex;
                            edge.mDstPass    = passIndex;
                            edge.mSrcQueue   = queue;
                            edge.mDstQueue   = queue;
                            edge.mCrossQueue = false;

                            FRhiTransitionInfo info{};
                            info.mResource = buffer;
                            info.mBefore   = state.mState;
                            info.mAfter    = access.mState;
                            edge.mInfos.PushBack(info);
                            edges.PushBack(edge);
                            passTransitions[passIndex].mSameQueueEdges.PushBack(
                                static_cast<u32>(edges.Size() - 1U));
                        }
                        state.mInitialized = true;
                        state.mState       = access.mState;
                        state.mQueue       = queue;
                        state.mLastPass    = passIndex;
                        continue;
                    }

                    if (state.mState == access.mState && state.mQueue == queue) {
                        state.mLastPass = passIndex;
                        continue;
                    }

                    FTransitionEdge edge{};
                    edge.mSrcPass    = state.mLastPass;
                    edge.mDstPass    = passIndex;
                    edge.mSrcQueue   = state.mQueue;
                    edge.mDstQueue   = queue;
                    edge.mCrossQueue = (state.mQueue != queue);

                    FRhiTransitionInfo info{};
                    info.mResource = buffer;
                    info.mBefore   = state.mState;
                    info.mAfter    = access.mState;
                    edge.mInfos.PushBack(info);
                    edges.PushBack(edge);
                    const u32 edgeIndex = static_cast<u32>(edges.Size() - 1U);
                    if (edge.mCrossQueue) {
                        passTransitions[edge.mSrcPass].mReleaseEdges.PushBack(edgeIndex);
                        passTransitions[edge.mDstPass].mAcquireEdges.PushBack(edgeIndex);
                    } else {
                        passTransitions[edge.mDstPass].mSameQueueEdges.PushBack(edgeIndex);
                    }

                    state.mState    = access.mState;
                    state.mQueue    = queue;
                    state.mLastPass = passIndex;
                }
            }
        }

        for (auto& edge : edges) {
            edge.mCreateInfo.mTransitions     = edge.mInfos.Data();
            edge.mCreateInfo.mTransitionCount = static_cast<u32>(edge.mInfos.Size());
            edge.mCreateInfo.mSrcQueue        = edge.mSrcQueue;
            edge.mCreateInfo.mDstQueue        = edge.mDstQueue;
            edge.mCreateInfo.mFlags           = 0U;

            if (edge.mCrossQueue) {
                Rhi::FRhiTransitionDesc desc{};
                desc.mSrcQueue   = edge.mSrcQueue;
                desc.mDstQueue   = edge.mDstQueue;
                edge.mTransition = mDevice->CreateTransition(desc);
                if (!edge.mTransition) {
                    Core::Logging::LogErrorCat(TEXT("RenderCore"),
                        "Failed to create FRhiTransition for cross-queue edge.");
                } else {
                    edge.mTransition->SetSignalValue(1ULL);
                }
                edge.mCreateInfo.mTransition = edge.mTransition.Get();
            }
        }

        FFrameGraphPassResources resources(graph);

        for (u32 passIndex = 0U; passIndex < static_cast<u32>(graph.mPasses.Size()); ++passIndex) {
            const auto passStart  = std::chrono::steady_clock::now();
            auto&      pass       = graph.mPasses[passIndex];
            const auto queue      = passQueues[passIndex];
            auto&      queueState = queues[GetQueueIndex(queue)];

            if (!queueState.mQueue || !queueState.mContext || queueState.mOps == nullptr) {
                continue;
            }

            const auto& transitions = passTransitions[passIndex];
            if (!transitions.mAcquireEdges.IsEmpty() && queueState.mHasCommands) {
                SubmitQueue(queueState);
            }

            BeginQueueIfNeeded(queueState);
            FRhiCmdContextAdapter    adapter(*queueState.mContext.Get(), *queueState.mOps);
            Core::Container::FString passMarkerText;
            passMarkerText.Assign(TEXT("FrameGraph.Pass"));
            if (pass.mDesc.mName != nullptr && pass.mDesc.mName[0] != '\0') {
                passMarkerText.Append(TEXT(":"));
                passMarkerText.Append(Core::Utility::String::FromUtf8Bytes(
                    pass.mDesc.mName, static_cast<usize>(std::strlen(pass.mDesc.mName))));
            }
            FRhiDebugMarker passMarker(adapter, passMarkerText.ToView());

            for (u32 edgeIndex : transitions.mAcquireEdges) {
                auto& edge = edges[edgeIndex];
                if (!edge.mTransition) {
                    continue;
                }
                FRhiQueueWait wait{};
                wait.mSemaphore = edge.mTransition->GetSemaphore();
                wait.mValue     = edge.mTransition->GetSignalValue();
                queueState.mPendingWaits.PushBack(wait);
            }

            for (u32 edgeIndex : transitions.mAcquireEdges) {
                auto& edge = edges[edgeIndex];
                adapter.RHIEndTransition(edge.mCreateInfo);
            }

            for (u32 edgeIndex : transitions.mSameQueueEdges) {
                auto& edge = edges[edgeIndex];
                adapter.RHIBeginTransition(edge.mCreateInfo);
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
                adapter.RHIBeginRenderPass(renderPassDesc);
            }

            if (pass.mExecute) {
                pass.mExecute(adapter, resources, pass.mPassData, pass.mExecuteUserData);
            } else if (pass.mDesc.mExecute) {
                pass.mDesc.mExecute(adapter, resources);
            }

            if (hasRenderPass) {
                adapter.RHIEndRenderPass();
            }

            if (!transitions.mReleaseEdges.IsEmpty()) {
                for (u32 edgeIndex : transitions.mReleaseEdges) {
                    auto& edge = edges[edgeIndex];
                    adapter.RHIBeginTransition(edge.mCreateInfo);
                }

                Core::Container::TVector<FRhiQueueSignal> signals;
                signals.Reserve(transitions.mReleaseEdges.Size());
                for (u32 edgeIndex : transitions.mReleaseEdges) {
                    auto& edge = edges[edgeIndex];
                    if (!edge.mTransition) {
                        continue;
                    }
                    FRhiQueueSignal signal{};
                    signal.mSemaphore = edge.mTransition->GetSemaphore();
                    signal.mValue     = edge.mTransition->GetSignalValue();
                    signals.PushBack(signal);
                }
                SubmitQueue(queueState, &signals);
                LogInfoCat(kFrameTimingCategory,
                    TEXT("FrameGraph.Pass index={} name={} queue={} ms={:.3f}"), passIndex,
                    (pass.mDesc.mName != nullptr) ? pass.mDesc.mName : "<unnamed>",
                    static_cast<u32>(queue), ElapsedMilliseconds(passStart));
                continue;
            }
            queueState.mHasCommands = true;
            LogInfoCat(kFrameTimingCategory,
                TEXT("FrameGraph.Pass index={} name={} queue={} ms={:.3f}"), passIndex,
                (pass.mDesc.mName != nullptr) ? pass.mDesc.mName : "<unnamed>",
                static_cast<u32>(queue), ElapsedMilliseconds(passStart));
        }

        // Apply external-output final transitions (for example backbuffer RenderTarget->Present).
        // The non-executor FrameGraph::Execute path performs these explicitly; keep executor
        // behavior consistent so presentable images are finalized reliably.
        if (!graph.mCompiledFinalTransitions.IsEmpty()) {
            auto& graphicsQueue = queues[GetQueueIndex(ERhiQueueType::Graphics)];
            if (graphicsQueue.mQueue && graphicsQueue.mContext && graphicsQueue.mOps != nullptr) {
                BeginQueueIfNeeded(graphicsQueue);
                FRhiCmdContextAdapter adapter(*graphicsQueue.mContext.Get(), *graphicsQueue.mOps);
                FRhiTransitionCreateInfo transition{};
                transition.mTransitions = graph.mCompiledFinalTransitions.Data();
                transition.mTransitionCount =
                    static_cast<u32>(graph.mCompiledFinalTransitions.Size());
                transition.mSrcQueue = ERhiQueueType::Graphics;
                transition.mDstQueue = ERhiQueueType::Graphics;
                adapter.RHIBeginTransition(transition);
                graphicsQueue.mHasCommands = true;
            }
        }

        for (auto& queueState : queues) {
            if (queueState.mRecording && queueState.mHasCommands) {
                SubmitQueue(queueState);
            }
        }
    }
} // namespace AltinaEngine::RenderCore
