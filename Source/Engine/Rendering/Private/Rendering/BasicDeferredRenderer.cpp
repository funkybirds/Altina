#include "Rendering/BasicDeferredRenderer.h"

namespace AltinaEngine::Rendering {
    void FBasicDeferredRenderer::PrepareForRendering(Rhi::FRhiDevice& device) {
        (void)device;
    }

    void FBasicDeferredRenderer::Render(RenderCore::FFrameGraph& graph) {
        (void)graph;
    }

    void FBasicDeferredRenderer::FinalizeRendering() {}
} // namespace AltinaEngine::Rendering
