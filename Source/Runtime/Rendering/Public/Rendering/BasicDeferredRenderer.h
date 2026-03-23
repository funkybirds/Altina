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
    class AE_RENDERING_API FBasicDeferredRenderer final : public IRenderer {
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

        void        PrepareForRendering(Rhi::FRhiDevice& device) override;
        void        Render(RenderCore::FFrameGraph& graph) override;
        void        FinalizeRendering() override;

    private:
        static constexpr u32                           kShadowCascades   = 4U;
        static constexpr u32                           kPerDrawFrameRing = 4U;

        Rhi::FRhiBufferRef                             mPerFrameBuffer;
        Rhi::FRhiBindGroupRef                          mPerFrameGroup;
        Rhi::FRhiBufferRef                             mPerDrawBuffer;
        Rhi::FRhiBindGroupRef                          mPerDrawGroup;
        u32                                            mPerDrawStrideBytes = 0U;
        u32                                            mPerDrawCapacity    = 0U;
        u32                                            mPerDrawWriteIndex  = 0U;
        u32                                            mPerDrawFrameSlot   = 0U;
        Rhi::FRhiBufferRef                             mIblConstantsBuffer;
        Rhi::FRhiBufferRef                             mSsaoConstantsBuffer;

        // D3D11 deferred context: updating the same cbuffer multiple times while recording can
        // make all draws see the "last written" data at Execute time. Use per-cascade cbuffers.
        TArray<Rhi::FRhiBufferRef, kShadowCascades>    mShadowPerFrameBuffers{};
        TArray<Rhi::FRhiBindGroupRef, kShadowCascades> mShadowPerFrameGroups{};
    };
} // namespace AltinaEngine::Rendering
