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
        Math::FVector3f mDirectionWS = Math::FVector3f(0.0f, -1.0f, 0.0f);

        // RGB linear color.
        Math::FVector3f mColor     = Math::FVector3f(1.0f, 1.0f, 1.0f);
        f32             mIntensity = 1.0f;

        bool            mCastShadows = false;

        // Cascaded shadow map (CSM) settings for this light (used when bCastShadows=true).
        // Kept on the light to allow per-scene tuning without global renderer hacks.
        u32             mShadowCascadeCount = 4U;     // [1,4]
        f32             mShadowSplitLambda  = 0.65f;  // [0,1]
        f32             mShadowMaxDistance  = 250.0f; // clamp view far (view-space Z)
        u32             mShadowMapSize      = 2048U;
        f32             mShadowReceiverBias = 0.0015f;
    };

    struct AE_RENDER_CORE_API FPointLight {
        Math::FVector3f mPositionWS = Math::FVector3f(0.0f, 0.0f, 0.0f);
        f32             mRange      = 5.0f;

        Math::FVector3f mColor     = Math::FVector3f(1.0f, 1.0f, 1.0f);
        f32             mIntensity = 1.0f;
    };

    struct AE_RENDER_CORE_API FLightSceneData {
        // Phase1: support a single main directional light + N point lights.
        bool                 mHasMainDirectionalLight = false;
        FDirectionalLight    mMainDirectionalLight{};

        TVector<FPointLight> mPointLights{};

        void                 Clear() {
            mHasMainDirectionalLight = false;
            mMainDirectionalLight    = {};
            mPointLights.Clear();
        }
    };
} // namespace AltinaEngine::RenderCore::Lighting
