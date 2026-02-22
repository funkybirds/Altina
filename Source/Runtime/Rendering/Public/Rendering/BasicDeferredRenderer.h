#pragma once

#include "Rendering/Renderer.h"
#include "Material/MaterialTemplate.h"
#include "Container/SmartPtr.h"
#include "Shader/ShaderRegistry.h"
#include "Rhi/RhiBindGroup.h"
#include "Rhi/RhiRefs.h"

namespace AltinaEngine::Rendering {
    class AE_RENDERING_API FBasicDeferredRenderer final : public IRenderer {
    public:
        [[nodiscard]] static auto GetDefaultMaterialTemplate()
            -> Core::Container::TShared<RenderCore::FMaterialTemplate>;
        static void SetDefaultMaterialTemplate(
            Core::Container::TShared<RenderCore::FMaterialTemplate> templ) noexcept;
        static void SetOutputShaderKeys(const RenderCore::FShaderRegistry::FShaderKey& vs,
            const RenderCore::FShaderRegistry::FShaderKey&                             ps) noexcept;
        [[nodiscard]] static auto RegisterShader(
            const RenderCore::FShaderRegistry::FShaderKey& key, Rhi::FRhiShaderRef shader) -> bool;

        void PrepareForRendering(Rhi::FRhiDevice& device) override;
        void Render(RenderCore::FFrameGraph& graph) override;
        void FinalizeRendering() override;

    private:
        Rhi::FRhiBufferRef    mPerFrameBuffer;
        Rhi::FRhiBindGroupRef mPerFrameGroup;
        Rhi::FRhiBufferRef    mPerDrawBuffer;
        Rhi::FRhiBindGroupRef mPerDrawGroup;
    };
} // namespace AltinaEngine::Rendering
