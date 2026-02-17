#pragma once

#include "Engine/EngineAPI.h"
#include "Engine/Runtime/MaterialCache.h"
#include "Engine/Runtime/SceneView.h"
#include "Render/DrawList.h"

namespace AltinaEngine::Engine {
    struct AE_ENGINE_API FSceneBatchBuildParams {
        RenderCore::EMaterialPass Pass          = RenderCore::EMaterialPass::BasePass;
        u32                       LodIndex      = 0U;
        bool                      bAllowInstancing = true;
    };

    /**
     * @brief Build draw lists from a render scene (StaticMesh only).
     */
    class AE_ENGINE_API FSceneBatchBuilder {
    public:
        void Build(const FRenderScene& scene, const FSceneView& view,
            const FSceneBatchBuildParams& params, FMaterialCache& materialCache,
            RenderCore::Render::FDrawList& outDrawList) const;
    };
} // namespace AltinaEngine::Engine
