#pragma once

#include "Deferred/DeferredCsm.h"

#include "Rendering/DrawListExecutor.h"

#include "Container/Deque.h"
#include "Types/Aliases.h"
#include "Utility/Assert.h"

namespace AltinaEngine::Rendering::Shadowing {
    template <typename TPipelineData, typename TBindingData> struct FDeferredCsmPassSetInputs {
        RenderCore::FFrameGraph*             mGraph              = nullptr;
        const RenderCore::View::FViewData*   mView               = nullptr;
        const RenderCore::Render::FDrawList* mShadowDrawLists[4] = {};
        const Deferred::FCsmBuildResult*     mCsm                = nullptr;

        FDrawListBindings                    mDrawBindings{};
        TPipelineData                        mShadowPipelineData{};
        TBindingData                         mBindingDataPerCascade[4]{};
        TBindingData                         mFallbackBindingData{};
        Rhi::FRhiBuffer*                     mCascadePerFrameBuffers[4] = {};
        Rhi::FRhiBindGroup*                  mCascadePerFrameGroups[4]  = {};
        Rhi::FRhiBuffer*                     mFallbackPerFrameBuffer    = nullptr;
        Rhi::FRhiBindGroup*                  mFallbackPerFrameGroup     = nullptr;

        Rhi::FRhiTextureRef*                 mPersistentShadowMap       = nullptr;
        u32*                                 mPersistentShadowMapSize   = nullptr;
        u32*                                 mPersistentShadowMapLayers = nullptr;

        FDrawPipelineResolver                mResolveShadowPipeline = nullptr;
        FDrawBatchBinder                     mBindPerDraw           = nullptr;
    };

    template <typename TPipelineData, typename TBindingData>
    void AddDeferredCsmPassSet(FDeferredCsmPassSetInputs<TPipelineData, TBindingData>& inputs,
        RenderCore::FFrameGraphTextureRef& outShadowMap) {
        using Core::Container::TDeque;
        using Core::Utility::DebugAssert;

        if (inputs.mGraph == nullptr || inputs.mView == nullptr || inputs.mCsm == nullptr
            || inputs.mResolveShadowPipeline == nullptr || inputs.mBindPerDraw == nullptr) {
            return;
        }

        struct FShadowCascadeExecuteContext {
            const RenderCore::Render::FDrawList* mShadowDrawLists[4] = {};
            FDrawListBindings                    mDrawBindings{};
            TPipelineData                        mShadowPipelineData{};
            TBindingData                         mBindingDataPerCascade[4]{};
            TBindingData                         mFallbackBindingData{};
            Rhi::FRhiBuffer*                     mCascadePerFrameBuffers[4] = {};
            Rhi::FRhiBindGroup*                  mCascadePerFrameGroups[4]  = {};
            Rhi::FRhiBuffer*                     mFallbackPerFrameBuffer    = nullptr;
            Rhi::FRhiBindGroup*                  mFallbackPerFrameGroup     = nullptr;
            FDrawPipelineResolver                mResolveShadowPipeline     = nullptr;
            FDrawBatchBinder                     mBindPerDraw               = nullptr;
        };

        static thread_local TDeque<FShadowCascadeExecuteContext> sShadowContexts;
        sShadowContexts.PushBack(FShadowCascadeExecuteContext{});
        auto& executeCtx                   = sShadowContexts.Back();
        executeCtx.mDrawBindings           = inputs.mDrawBindings;
        executeCtx.mShadowPipelineData     = inputs.mShadowPipelineData;
        executeCtx.mFallbackBindingData    = inputs.mFallbackBindingData;
        executeCtx.mFallbackPerFrameBuffer = inputs.mFallbackPerFrameBuffer;
        executeCtx.mFallbackPerFrameGroup  = inputs.mFallbackPerFrameGroup;
        executeCtx.mResolveShadowPipeline  = inputs.mResolveShadowPipeline;
        executeCtx.mBindPerDraw            = inputs.mBindPerDraw;
        for (u32 i = 0U; i < 4U; ++i) {
            executeCtx.mShadowDrawLists[i]        = inputs.mShadowDrawLists[i];
            executeCtx.mBindingDataPerCascade[i]  = inputs.mBindingDataPerCascade[i];
            executeCtx.mCascadePerFrameBuffers[i] = inputs.mCascadePerFrameBuffers[i];
            executeCtx.mCascadePerFrameGroups[i]  = inputs.mCascadePerFrameGroups[i];
        }

        Deferred::FCsmShadowPassInputs shadowInputs{};
        shadowInputs.Graph                     = inputs.mGraph;
        shadowInputs.View                      = inputs.mView;
        shadowInputs.Csm                       = inputs.mCsm;
        shadowInputs.PersistentShadowMap       = inputs.mPersistentShadowMap;
        shadowInputs.PersistentShadowMapSize   = inputs.mPersistentShadowMapSize;
        shadowInputs.PersistentShadowMapLayers = inputs.mPersistentShadowMapLayers;
        for (u32 i = 0U; i < 4U; ++i) {
            shadowInputs.ShadowDrawLists[i] = inputs.mShadowDrawLists[i];
        }

        shadowInputs.ExecuteCascadeFn = [](Rhi::FRhiCmdContext& ctx, u32 cascadeIndex,
                                            const Core::Math::FMatrix4x4f& lightViewProj,
                                            u32 shadowMapSize, void* userData) -> void {
            auto* executeData = static_cast<FShadowCascadeExecuteContext*>(userData);
            if (executeData == nullptr) {
                return;
            }

            Rhi::FRhiBuffer*    perFrameBuffer = (cascadeIndex < 4U)
                   ? executeData->mCascadePerFrameBuffers[cascadeIndex]
                   : executeData->mFallbackPerFrameBuffer;
            Rhi::FRhiBindGroup* perFrameGroup  = (cascadeIndex < 4U)
                 ? executeData->mCascadePerFrameGroups[cascadeIndex]
                 : executeData->mFallbackPerFrameGroup;
            if (reinterpret_cast<usize>(perFrameBuffer) == static_cast<usize>(~0ULL)
                || reinterpret_cast<usize>(perFrameGroup) == static_cast<usize>(~0ULL)) {
                DebugAssert(false, TEXT("BasicDeferredRenderer"),
                    "Shadow cascade execute has poisoned pointers (cascade={}, perFrameBuffer={}, perFrameGroup={}).",
                    cascadeIndex, reinterpret_cast<usize>(perFrameBuffer),
                    reinterpret_cast<usize>(perFrameGroup));
                return;
            }

            if (perFrameBuffer != nullptr) {
                Deferred::FPerFrameConstants constants{};
                constants.ViewProjection = lightViewProj;
                ctx.RHIUpdateDynamicBufferDiscard(
                    perFrameBuffer, &constants, sizeof(constants), 0ULL);
            }
            if (perFrameBuffer == nullptr || perFrameGroup == nullptr) {
                return;
            }

            Rhi::FRhiViewportRect viewport{};
            viewport.mX        = 0.0f;
            viewport.mY        = 0.0f;
            viewport.mWidth    = static_cast<f32>(shadowMapSize);
            viewport.mHeight   = static_cast<f32>(shadowMapSize);
            viewport.mMinDepth = 0.0f;
            viewport.mMaxDepth = 1.0f;
            ctx.RHISetViewport(viewport);

            Rhi::FRhiScissorRect scissor{};
            scissor.mX      = 0;
            scissor.mY      = 0;
            scissor.mWidth  = shadowMapSize;
            scissor.mHeight = shadowMapSize;
            ctx.RHISetScissor(scissor);

            const auto* shadowDrawList =
                (cascadeIndex < 4U) ? executeData->mShadowDrawLists[cascadeIndex] : nullptr;
            if (shadowDrawList != nullptr) {
                auto bindings     = executeData->mDrawBindings;
                bindings.PerFrame = perFrameGroup;
                auto* bindingData = (cascadeIndex < 4U)
                    ? &executeData->mBindingDataPerCascade[cascadeIndex]
                    : &executeData->mFallbackBindingData;
                FDrawListExecutor::ExecuteBasePass(ctx, *shadowDrawList, bindings,
                    executeData->mResolveShadowPipeline, &executeData->mShadowPipelineData,
                    executeData->mBindPerDraw, bindingData);
            }
        };
        shadowInputs.ExecuteCascadeUserData = &executeCtx;
        Deferred::AddCsmShadowPasses(shadowInputs, outShadowMap);
    }
} // namespace AltinaEngine::Rendering::Shadowing
