#pragma once

#include "Rendering/RenderingAPI.h"

#include "FrameGraph/FrameGraph.h"
#include "Rhi/RhiFwd.h"

namespace AltinaEngine::RenderCore::View {
    struct FViewData;
}

namespace AltinaEngine::RenderCore::Lighting {
    struct FLightSceneData;
}

namespace AltinaEngine::RenderCore::Render {
    struct FDrawList;
}

namespace AltinaEngine::Rendering {
    struct AE_RENDERING_API FRenderViewContext {
        // Stable per-view key (e.g. ViewTarget + CameraId), used for persistent temporal state.
        u64                                          ViewKey      = 0ULL;
        const RenderCore::View::FViewData*           View         = nullptr;
        const RenderCore::Render::FDrawList*         DrawList     = nullptr;
        Rhi::FRhiTexture*                            OutputTarget = nullptr;

        // Deferred lighting input.
        const RenderCore::Lighting::FLightSceneData* Lights = nullptr;

        // Optional shadow caster list (e.g., directional CSM).
        const RenderCore::Render::FDrawList*         ShadowDrawList = nullptr;
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
