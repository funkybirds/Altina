#pragma once

#include "DebugGui/Core/DebugGuiCoreTypes.h"
#include "DebugGui/Core/FontAtlas.h"

#include "Rhi/RhiBindGroup.h"
#include "Rhi/RhiBindGroupLayout.h"
#include "Rhi/RhiBuffer.h"
#include "Rhi/RhiPipeline.h"
#include "Rhi/RhiPipelineLayout.h"
#include "Rhi/RhiResourceView.h"
#include "Rhi/RhiSampler.h"
#include "Rhi/RhiShader.h"
#include "Rhi/RhiStructs.h"
#include "Rhi/RhiTexture.h"

#include "Shader/ShaderBindingUtility.h"

namespace AltinaEngine::Rhi {
    class FRhiDevice;
    class FRhiViewport;
} // namespace AltinaEngine::Rhi

namespace AltinaEngine::DebugGui::Private {
    class FDebugGuiRendererD3D11 {
    public:
        void Render(Rhi::FRhiDevice& device, Rhi::FRhiViewport& viewport, const FDrawData& drawData,
            const FFontAtlas& atlas);

    private:
        struct FConstants {
            f32 ScaleX     = 1.0f;
            f32 ScaleY     = 1.0f;
            f32 TranslateX = -1.0f;
            f32 TranslateY = -1.0f;
        };

        bool EnsureResources(Rhi::FRhiDevice& device, const FFontAtlas& atlas);
        bool EnsureBackBufferRtv(Rhi::FRhiDevice& device, Rhi::FRhiTexture* backBuffer);
        bool EnsureAuxiliaryRtvs(
            Rhi::FRhiDevice& device, u32 width, u32 height, Rhi::ERhiFormat format);
        bool EnsureGeometryBuffers(Rhi::FRhiDevice& device, const FDrawData& drawData);
        void UpdateConstants(u32 w, u32 h);
        bool CompileShaders(Rhi::FRhiDevice& device);

        Rhi::FRhiTextureRef                            mFontTexture;
        Rhi::FRhiShaderResourceViewRef                 mFontSrv;
        Rhi::FRhiSamplerRef                            mSampler;
        Rhi::FRhiBufferRef                             mConstantsBuffer;
        Rhi::FRhiBindGroupLayoutRef                    mLayout;
        Rhi::FRhiPipelineLayoutRef                     mPipelineLayout;
        Rhi::FRhiBindGroupRef                          mBindGroup;
        Rhi::FRhiShaderRef                             mVertexShader;
        Rhi::FRhiShaderRef                             mPixelShader;
        Rhi::FRhiPipelineRef                           mPipeline;
        RenderCore::ShaderBinding::FBindingLookupTable mBindingLookupTable;

        Rhi::FRhiTexture*                              mBackBufferTex = nullptr;
        Rhi::FRhiRenderTargetViewRef                   mBackBufferRtv;
        Rhi::FRhiTextureRef                            mAuxColorTex1;
        Rhi::FRhiTextureRef                            mAuxColorTex2;
        Rhi::FRhiRenderTargetViewRef                   mAuxColorRtv1;
        Rhi::FRhiRenderTargetViewRef                   mAuxColorRtv2;
        u32                                            mAuxWidth  = 0U;
        u32                                            mAuxHeight = 0U;
        Rhi::ERhiFormat                                mAuxFormat = Rhi::ERhiFormat::Unknown;

        Rhi::FRhiBufferRef                             mVertexBuffer;
        Rhi::FRhiBufferRef                             mIndexBuffer;
        u64                                            mVertexBufferSize = 0ULL;
        u64                                            mIndexBufferSize  = 0ULL;
    };
} // namespace AltinaEngine::DebugGui::Private
