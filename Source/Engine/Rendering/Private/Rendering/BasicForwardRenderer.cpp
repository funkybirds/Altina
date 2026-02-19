#include "Rendering/BasicForwardRenderer.h"

namespace AltinaEngine::Rendering {
    void FBasicForwardRenderer::PrepareForRendering(Rhi::FRhiDevice& device) { (void)device; }

    void FBasicForwardRenderer::Render(RenderCore::FFrameGraph& graph) { (void)graph; }

    void FBasicForwardRenderer::FinalizeRendering() {}
} // namespace AltinaEngine::Rendering
