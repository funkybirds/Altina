#pragma once

#include "Rendering/RenderingAPI.h"

#include "FrameGraph/FrameGraph.h"
#include "Rhi/RhiFwd.h"

namespace AltinaEngine::Rendering {
    /**
     * @brief Renderer interface for pass assembly and lifetime management.
     */
    class AE_RENDERING_API IRenderer {
    public:
        virtual ~IRenderer() = default;

        // Resource allocation/setup before rendering.
        virtual void PrepareForRendering(Rhi::FRhiDevice& device) = 0;

        // Assemble passes into the frame graph.
        virtual void Render(RenderCore::FFrameGraph& graph) = 0;

        // Resource cleanup after rendering.
        virtual void FinalizeRendering() = 0;
    };

    class AE_RENDERING_API FBasicForwardRenderer;
    class AE_RENDERING_API FBasicDeferredRenderer;
} // namespace AltinaEngine::Rendering
