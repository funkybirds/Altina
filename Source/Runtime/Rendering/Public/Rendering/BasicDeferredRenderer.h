#pragma once

#include "Rendering/Renderer.h"
#include "Material/MaterialTemplate.h"
#include "Container/SmartPtr.h"
#include "Shader/ShaderRegistry.h"
#include "Rhi/RhiBindGroup.h"
#include "Rhi/RhiRefs.h"
#include "Container/Array.h"

using AltinaEngine::Core::Container::TArray;

namespace AltinaEngine::Rendering {
    class AE_RENDERING_API FBasicDeferredRenderer final : public FBaseRenderer {
    public:
        [[nodiscard]] static auto GetDefaultMaterialTemplate()
            -> Core::Container::TShared<RenderCore::FMaterialTemplate>;
        static void SetDefaultMaterialTemplate(
            Core::Container::TShared<RenderCore::FMaterialTemplate> templ) noexcept;
        static void SetOutputShaderKeys(const RenderCore::FShaderRegistry::FShaderKey& vs,
            const RenderCore::FShaderRegistry::FShaderKey&                             ps) noexcept;
        static void SetLightingShaderKeys(const RenderCore::FShaderRegistry::FShaderKey& vs,
            const RenderCore::FShaderRegistry::FShaderKey& ps) noexcept;
        static void SetSsaoShaderKeys(const RenderCore::FShaderRegistry::FShaderKey& vs,
            const RenderCore::FShaderRegistry::FShaderKey&                           ps) noexcept;
        static void SetSkyBoxShaderKeys(const RenderCore::FShaderRegistry::FShaderKey& vs,
            const RenderCore::FShaderRegistry::FShaderKey&                             ps) noexcept;
        static void SetAtmosphereSkyShaderKeys(const RenderCore::FShaderRegistry::FShaderKey& vs,
            const RenderCore::FShaderRegistry::FShaderKey& ps) noexcept;
        [[nodiscard]] static auto RegisterShader(
            const RenderCore::FShaderRegistry::FShaderKey& key, Rhi::FRhiShaderRef shader) -> bool;
        static void ShutdownSharedResources() noexcept;

        // Debug: use Lambert (diffuse-only) shading in PSDeferredLighting instead of PBR.
        static void SetDeferredLightingDebugLambert(bool bEnabled) noexcept;

        void        SetViewContext(const FRenderViewContext& context) override;
        void        PrepareForRendering(Rhi::FRhiDevice& device) override;
        void        FinalizeRendering() override;

    protected:
        void RegisterBuiltinPasses() override;

    private:
        static constexpr u32 kShadowCascades    = 4U;
        static constexpr u32 kInstanceFrameRing = 4U;
        void                 RegisterDeferredGBufferBasePass(RenderCore::FFrameGraph& graph);
        void                 RegisterDeferredShadowPass(RenderCore::FFrameGraph& graph);
        void                 RegisterDeferredSsaoPass(RenderCore::FFrameGraph& graph);
        void                 RegisterDeferredLightingPass(RenderCore::FFrameGraph& graph);
        void                 RegisterDeferredSkyPass(RenderCore::FFrameGraph& graph);
        void                 RegisterDeferredPostProcessPass(RenderCore::FFrameGraph& graph);

        struct FDeferredGraphOutputs {
            RenderCore::FFrameGraphTextureRef mGBufferA{};
            RenderCore::FFrameGraphTextureRef mGBufferB{};
            RenderCore::FFrameGraphTextureRef mGBufferC{};
            RenderCore::FFrameGraphTextureRef mSceneDepth{};
            RenderCore::FFrameGraphTextureRef mShadowMap{};
            RenderCore::FFrameGraphTextureRef mSsaoTexture{};
            RenderCore::FFrameGraphTextureRef mSceneColorHdr{};
        };

        Rhi::FRhiBufferRef                                mPerFrameBuffer;
        Rhi::FRhiBindGroupRef                             mPerFrameGroup;
        Rhi::FRhiBufferRef                                mPerDrawBuffer;
        TArray<Rhi::FRhiBindGroupRef, kInstanceFrameRing> mPerDrawGroups{};
        u32                                               mPerDrawStrideBytes = 0U;
        u32                                               mPerDrawCapacity    = 0U;
        u32                                               mPerDrawFrameSlot   = 0U;
        Rhi::FRhiBufferRef                                mIblConstantsBuffer;
        Rhi::FRhiBufferRef                                mSsaoConstantsBuffer;

        // D3D11 deferred context: updating the same cbuffer multiple times while recording can
        // make all draws see the "last written" data at Execute time. Use per-cascade cbuffers.
        TArray<Rhi::FRhiBufferRef, kShadowCascades>       mShadowPerFrameBuffers{};
        TArray<Rhi::FRhiBindGroupRef, kShadowCascades>    mShadowPerFrameGroups{};
        FDeferredGraphOutputs                             mGraphOutputs{};
    };
} // namespace AltinaEngine::Rendering
