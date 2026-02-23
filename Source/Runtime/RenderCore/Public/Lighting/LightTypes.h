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
