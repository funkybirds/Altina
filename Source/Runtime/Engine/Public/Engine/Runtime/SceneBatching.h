#pragma once

#include "Engine/EngineAPI.h"
#include "Container/String.h"
#include "Engine/Runtime/MaterialCache.h"
#include "Engine/Runtime/SceneView.h"
#include "Render/DrawList.h"

namespace AltinaEngine::Engine {
    struct AE_ENGINE_API FSceneBatchBuildParams {
        RenderCore::EMaterialPass Pass                  = RenderCore::EMaterialPass::BasePass;
        u32                       LodIndex              = 0U;
        bool                      bAllowInstancing      = true;
        bool                      bEnableFrustumCulling = true;
        bool                      bEnableShadowDistanceCulling    = false;
        f32                       mShadowCullMaxViewDepth         = 0.0f;
        f32                       mShadowCullViewDepthPadding     = 0.0f;
        bool                      bEnableShadowSmallCasterCulling = false;
        f32                       mShadowMinCasterRadiusWs        = 0.0f;
        Core::Container::FString  mDebugName{};
        Core::Math::FMatrix4x4f   mCullViewProj        = Core::Math::FMatrix4x4f(0.0f);
        bool                      bUseCustomCullMatrix = false;
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
