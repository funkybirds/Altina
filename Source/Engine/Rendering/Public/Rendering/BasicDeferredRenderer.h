#pragma once

#include "Rendering/Renderer.h"

namespace AltinaEngine::Rendering {
    class AE_RENDERING_API FBasicDeferredRenderer final : public IRenderer {
    public:
        void PrepareForRendering(Rhi::FRhiDevice& device) override;
        void Render(RenderCore::FFrameGraph& graph) override;
        void FinalizeRendering() override;
    };
} // namespace AltinaEngine::Rendering
