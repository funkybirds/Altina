#include "Rendering/GraphicsPipelinePreset.h"

namespace AltinaEngine::Rendering {
    namespace {
        void ApplyStateOverrides(const FRendererGraphicsPipelineStateOverrides& overrides,
            Rhi::FRhiGraphicsPipelineDesc&                                      outDesc) {
            if (overrides.mHasRasterState) {
                outDesc.mRasterState = overrides.mRasterState;
            }
            if (overrides.mHasDepthState) {
                outDesc.mDepthState = overrides.mDepthState;
            }
            if (overrides.mHasBlendState) {
                outDesc.mBlendState = overrides.mBlendState;
            }
        }
    } // namespace

    auto BuildGraphicsPipelineDesc(const FRendererGraphicsPipelineBuildInputs& inputs,
        Rhi::FRhiGraphicsPipelineDesc&                                         outDesc) -> bool {
        outDesc = {};
        outDesc.mDebugName.Assign(inputs.mDebugName);
        outDesc.mPipelineLayout = inputs.mPipelineLayout;
        outDesc.mVertexShader   = inputs.mVertexShader;
        outDesc.mPixelShader    = inputs.mPixelShader;
        outDesc.mGeometryShader = inputs.mGeometryShader;
        outDesc.mHullShader     = inputs.mHullShader;
        outDesc.mDomainShader   = inputs.mDomainShader;
        if (inputs.mVertexLayout != nullptr) {
            outDesc.mVertexLayout = *inputs.mVertexLayout;
        }

        switch (inputs.mPreset) {
            case ERendererGraphicsPipelinePreset::MeshMaterial:
            {
                if (inputs.mMaterialPassState == nullptr) {
                    return false;
                }
                outDesc.mRasterState = inputs.mMaterialPassState->mRaster;
                outDesc.mDepthState  = inputs.mMaterialPassState->mDepth;
                outDesc.mBlendState  = inputs.mMaterialPassState->mBlend;
                break;
            }
            case ERendererGraphicsPipelinePreset::Fullscreen:
            {
                outDesc.mRasterState             = {};
                outDesc.mDepthState              = {};
                outDesc.mBlendState              = {};
                outDesc.mRasterState.mCullMode   = Rhi::ERhiRasterCullMode::None;
                outDesc.mDepthState.mDepthEnable = false;
                outDesc.mDepthState.mDepthWrite  = false;
                break;
            }
            case ERendererGraphicsPipelinePreset::ShadowDepth:
            {
                outDesc.mRasterState              = {};
                outDesc.mDepthState               = {};
                outDesc.mBlendState               = {};
                outDesc.mRasterState.mCullMode    = Rhi::ERhiRasterCullMode::None;
                outDesc.mDepthState.mDepthEnable  = true;
                outDesc.mDepthState.mDepthWrite   = true;
                outDesc.mDepthState.mDepthCompare = Rhi::ERhiCompareOp::GreaterEqual;
                if (inputs.mMaterialPassState != nullptr) {
                    outDesc.mRasterState = inputs.mMaterialPassState->mRaster;
                    outDesc.mDepthState  = inputs.mMaterialPassState->mDepth;
                    outDesc.mBlendState  = inputs.mMaterialPassState->mBlend;
                }
                break;
            }
        }

        ApplyStateOverrides(inputs.mStateOverrides, outDesc);
        return true;
    }
} // namespace AltinaEngine::Rendering
