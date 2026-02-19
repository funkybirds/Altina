#pragma once

#include "Rendering/RenderingAPI.h"

#include "FrameGraph/FrameGraph.h"
#include "Rhi/RhiFwd.h"

namespace AltinaEngine::RenderCore::View {
    struct FViewData;
}

namespace AltinaEngine::RenderCore::Render {
    struct FDrawList;
}

namespace AltinaEngine::Rendering {
    struct AE_RENDERING_API FRenderViewContext {
        const RenderCore::View::FViewData*   View         = nullptr;
        const RenderCore::Render::FDrawList* DrawList     = nullptr;
        Rhi::FRhiTexture*                    OutputTarget = nullptr;
    };

    /**
     * @brief Renderer interface for pass assembly and lifetime management.
     */
    class AE_RENDERING_API IRenderer {
    public:
        virtual ~IRenderer() = default;

        virtual void SetViewContext(const FRenderViewContext& context) { mViewContext = context; }

        // Resource allocation/setup before rendering.
        virtual void PrepareForRendering(Rhi::FRhiDevice& device) = 0;

        // Assemble passes into the frame graph.
        virtual void Render(RenderCore::FFrameGraph& graph) = 0;

        // Resource cleanup after rendering.
        virtual void FinalizeRendering() = 0;

    protected:
        FRenderViewContext mViewContext{};
    };

    class AE_RENDERING_API FBasicForwardRenderer;
    class AE_RENDERING_API FBasicDeferredRenderer;
} // namespace AltinaEngine::Rendering
