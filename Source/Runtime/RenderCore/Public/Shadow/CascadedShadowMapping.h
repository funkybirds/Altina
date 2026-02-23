#pragma once

#include "RenderCoreAPI.h"

#include "Lighting/LightTypes.h"
#include "Math/Matrix.h"
#include "Math/Vector.h"
#include "Types/Aliases.h"
#include "View/ViewData.h"

namespace AltinaEngine::RenderCore::Shadow {
    namespace Math = Core::Math;

    inline constexpr u32 kMaxCascades = 4U;

    struct AE_RENDER_CORE_API FCSMSettings {
        u32 CascadeCount = 4U;     // [1,4]
        f32 SplitLambda  = 0.5f;   // [0,1]
        f32 MaxDistance  = 100.0f; // clamp view far

        u32 ShadowMapSize = 2048U;

        // Depth bias applied in shading (receiver bias in NDC depth).
        f32 ReceiverBias = 0.0015f;
    };

    struct AE_RENDER_CORE_API FCSMCascade {
        Math::FMatrix4x4f LightViewProj = Math::FMatrix4x4f(0.0f);
        // View-space split range for cascade selection (x=near, y=far).
        Math::FVector2f   SplitVS = Math::FVector2f(0.0f, 0.0f);
    };

    struct AE_RENDER_CORE_API FCSMData {
        u32         CascadeCount = 0U;
        FCSMCascade Cascades[kMaxCascades]{};
    };

    /**
     * @brief Build CSM matrices + splits for a directional light.
     *
     * Notes:
     * - Assumes LH coordinate system (camera forward +Z).
     * - Produces reversed-Z orthographic projections for consistency with the engine's default
     *   depth convention.
     */
    AE_RENDER_CORE_API void BuildDirectionalCSM(const View::FViewData& view,
        const Lighting::FDirectionalLight& light, const FCSMSettings& settings, FCSMData& outCsm);
} // namespace AltinaEngine::RenderCore::Shadow
