#pragma once

#include "DebugGui/Core/DebugGuiCoreTypes.h"
#include "DebugGui/Core/FontAtlas.h"
#include "DebugGui/Core/IconAtlas.h"

#include "Container/HashMap.h"

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
        using FImageTextureMap = Core::Container::THashMap<u64, Rhi::FRhiTexture*>;

        void SetExternalTextures(const FImageTextureMap& textures);
        void Render(Rhi::FRhiDevice& device, Rhi::FRhiViewport& viewport, const FDrawData& drawData,
            const FFontAtlas& atlas, const FIconAtlas& iconAtlas);

    private:
        struct FConstants {
            f32 ScaleX        = 1.0f;
            f32 ScaleY        = 1.0f;
            f32 TranslateX    = -1.0f;
            f32 TranslateY    = -1.0f;
            f32 SdfEdge       = 0.5f;
            f32 SdfSoftness   = 0.0f;
            f32 SdfPixelRange = FFontAtlas::kSdfPixelRange;
            f32 AtlasWidth    = static_cast<f32>(FFontAtlas::kAtlasW);
            f32 AtlasHeight   = static_cast<f32>(FFontAtlas::kAtlasH);
            f32 UseSdf        = 1.0f;
            f32 FontStretchX  = 2.0f;
            f32 GlyphTexelW   = static_cast<f32>(FFontAtlas::kAtlasGlyphW);
            f32 GlyphTexelH   = static_cast<f32>(FFontAtlas::kAtlasGlyphH);
        };

        bool EnsureResources(
            Rhi::FRhiDevice& device, const FFontAtlas& atlas, const FIconAtlas& iconAtlas);
        bool EnsureBackBufferRtv(Rhi::FRhiDevice& device, Rhi::FRhiTexture* backBuffer);
        bool EnsureAuxiliaryRtvs(
            Rhi::FRhiDevice& device, u32 width, u32 height, Rhi::ERhiFormat format);
        bool EnsureGeometryBuffers(Rhi::FRhiDevice& device, const FDrawData& drawData);
        bool EnsureBindGroupForTexture(Rhi::FRhiDevice& device, u64 imageId,
            Rhi::FRhiTexture* texture, Rhi::FRhiBindGroupRef& out);
        void PruneExternalTextureCache();
        void UpdateConstants(u32 w, u32 h, const FDrawData& drawData, const FFontAtlas& atlas,
            const FIconAtlas& iconAtlas);
        bool CompileShaders(Rhi::FRhiDevice& device);

        Rhi::FRhiTextureRef                            mFontTexture;
        Rhi::FRhiTextureRef                            mIconTexture;
        Rhi::FRhiShaderResourceViewRef                 mFontSrv;
        Rhi::FRhiShaderResourceViewRef                 mIconSrv;
        Rhi::FRhiSamplerRef                            mSampler;
        Rhi::FRhiBufferRef                             mConstantsBufferSdf;
        Rhi::FRhiBufferRef                             mConstantsBufferIcon;
        Rhi::FRhiBufferRef                             mConstantsBufferImage;
        Rhi::FRhiBindGroupLayoutRef                    mLayout;
        Rhi::FRhiPipelineLayoutRef                     mPipelineLayout;
        Rhi::FRhiBindGroupRef                          mFontBindGroup;
        Rhi::FRhiBindGroupRef                          mIconBindGroup;
        Rhi::FRhiShaderRef                             mVertexShader;
        Rhi::FRhiShaderRef                             mPixelShader;
        Rhi::FRhiPipelineRef                           mPipeline;
        RenderCore::ShaderBinding::FBindingLookupTable mBindingLookupTable;
        u32                          mConstantsBinding = RenderCore::ShaderBinding::kInvalidBinding;
        u32                          mTextureBinding   = RenderCore::ShaderBinding::kInvalidBinding;
        u32                          mSamplerBinding   = RenderCore::ShaderBinding::kInvalidBinding;

        Rhi::FRhiTexture*            mBackBufferTex = nullptr;
        Rhi::FRhiRenderTargetViewRef mBackBufferRtv;
        Rhi::FRhiTextureRef          mAuxColorTex1;
        Rhi::FRhiTextureRef          mAuxColorTex2;
        Rhi::FRhiRenderTargetViewRef mAuxColorRtv1;
        Rhi::FRhiRenderTargetViewRef mAuxColorRtv2;
        u32                          mAuxWidth  = 0U;
        u32                          mAuxHeight = 0U;
        Rhi::ERhiFormat              mAuxFormat = Rhi::ERhiFormat::Unknown;

        Rhi::FRhiBufferRef           mVertexBuffer;
        Rhi::FRhiBufferRef           mIndexBuffer;
        FConstants                   mConstantsSdfValue{};
        FConstants                   mConstantsIconValue{};
        FConstants                   mConstantsImageValue{};
        u64                          mVertexBufferSize = 0ULL;
        u64                          mIndexBufferSize  = 0ULL;

        struct FExternalTextureBinding {
            Rhi::FRhiTexture*              Texture = nullptr;
            Rhi::FRhiShaderResourceViewRef Srv;
            Rhi::FRhiBindGroupRef          BindGroup;
        };

        FImageTextureMap                                        mExternalTextures;
        Core::Container::THashMap<u64, FExternalTextureBinding> mExternalTextureCache;
    };
} // namespace AltinaEngine::DebugGui::Private
