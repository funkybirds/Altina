#pragma once

#include "Rendering/RenderingAPI.h"

#include "Container/StringView.h"
#include "Material/MaterialPass.h"
#include "Rhi/RhiStructs.h"
#include "Types/Aliases.h"

namespace AltinaEngine::Rendering {
    using Core::Container::FStringView;

    enum class ERendererGraphicsPipelinePreset : u8 {
        MeshMaterial = 0U,
        Fullscreen   = 1U,
        ShadowDepth  = 2U
    };

    struct AE_RENDERING_API FRendererGraphicsPipelineStateOverrides {
        bool                     mHasRasterState = false;
        Rhi::FRhiRasterStateDesc mRasterState{};

        bool                     mHasDepthState = false;
        Rhi::FRhiDepthStateDesc  mDepthState{};

        bool                     mHasBlendState = false;
        Rhi::FRhiBlendStateDesc  mBlendState{};
    };

    struct AE_RENDERING_API FRendererGraphicsPipelineBuildInputs {
        ERendererGraphicsPipelinePreset  mPreset = ERendererGraphicsPipelinePreset::MeshMaterial;
        FStringView                      mDebugName{};
        Rhi::FRhiPipelineLayout*         mPipelineLayout           = nullptr;
        Rhi::FRhiShader*                 mVertexShader             = nullptr;
        Rhi::FRhiShader*                 mPixelShader              = nullptr;
        Rhi::FRhiShader*                 mGeometryShader           = nullptr;
        Rhi::FRhiShader*                 mHullShader               = nullptr;
        Rhi::FRhiShader*                 mDomainShader             = nullptr;
        const Rhi::FRhiVertexLayoutDesc* mVertexLayout             = nullptr;
        const RenderCore::FMaterialPassState*   mMaterialPassState = nullptr;
        FRendererGraphicsPipelineStateOverrides mStateOverrides{};
    };

    [[nodiscard]] AE_RENDERING_API auto BuildGraphicsPipelineDesc(
        const FRendererGraphicsPipelineBuildInputs& inputs, Rhi::FRhiGraphicsPipelineDesc& outDesc)
        -> bool;
} // namespace AltinaEngine::Rendering
