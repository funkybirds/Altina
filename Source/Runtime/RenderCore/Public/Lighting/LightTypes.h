#pragma once

#include "RenderCoreAPI.h"

#include "Container/Vector.h"
#include "Math/Vector.h"
#include "Types/Aliases.h"

namespace AltinaEngine::RenderCore::Lighting {
    namespace Container = Core::Container;
    using Container::TVector;
    namespace Math = Core::Math;

    enum class ELightType : u8 {
        Directional = 0,
        Point
    };

    struct AE_RENDER_CORE_API FDirectionalLight {
        // Light propagation direction (from light towards the scene), world space.
        Math::FVector3f DirectionWS = Math::FVector3f(0.0f, -1.0f, 0.0f);

        // RGB linear color.
        Math::FVector3f Color     = Math::FVector3f(1.0f, 1.0f, 1.0f);
        f32             Intensity = 1.0f;

        bool            bCastShadows = false;

        // Cascaded shadow map (CSM) settings for this light (used when bCastShadows=true).
        // Kept on the light to allow per-scene tuning without global renderer hacks.
        u32             ShadowCascadeCount = 4U;     // [1,4]
        f32             ShadowSplitLambda  = 0.65f;  // [0,1]
        f32             ShadowMaxDistance  = 250.0f; // clamp view far (view-space Z)
        u32             ShadowMapSize      = 2048U;
        f32             ShadowReceiverBias = 0.0015f;
    };

    struct AE_RENDER_CORE_API FPointLight {
        Math::FVector3f PositionWS = Math::FVector3f(0.0f, 0.0f, 0.0f);
        f32             Range      = 5.0f;

        Math::FVector3f Color     = Math::FVector3f(1.0f, 1.0f, 1.0f);
        f32             Intensity = 1.0f;
    };

    struct AE_RENDER_CORE_API FLightSceneData {
        // Phase1: support a single main directional light + N point lights.
        bool                 bHasMainDirectionalLight = false;
        FDirectionalLight    MainDirectionalLight{};

        TVector<FPointLight> PointLights{};

        void                 Clear() {
            bHasMainDirectionalLight = false;
            MainDirectionalLight     = {};
            PointLights.Clear();
        }
    };
} // namespace AltinaEngine::RenderCore::Lighting
