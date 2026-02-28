#pragma once

#include "Types/Aliases.h"

namespace AltinaEngine::Demo::SpaceshipGame::NativeScripts {
    struct FSpaceshipConstants final {
        // Scale: 1 unit = 10,000 km (approx). Some values are intentionally compressed.
        static constexpr f32 EarthRadius = 0.6371f;
        static constexpr f32 MoonRadius  = 0.1737f;
        static constexpr f32 SunRadius   = 6.9634f;

        static constexpr f32 EarthMoonDistance = 38.44f;
        static constexpr f32 SunEarthDistance  = 400.0f;

        // Ship/orbit tuning (demo constants).
        static constexpr f32 ShipRadius       = 0.05f;
        static constexpr f32 EarthOrbitRadius = 1.2f;
        static constexpr f32 MoonOrbitRadius  = 0.55f;

        // Revolution speeds (radians/sec). Not physical; tuned for readability.
        static constexpr f32 EarthRevolutionSpeed = 0.03f;
        static constexpr f32 MoonRevolutionSpeed  = 0.25f;

        static constexpr f32 TransferWindowHalfAngleRad  = 15.0f * (3.14159265f / 180.0f);
        static constexpr f32 TransferMoonCaptureDistance = 1.6f;
        static constexpr f32 OrbitSwitchEpsilon          = 0.15f;
    };
} // namespace AltinaEngine::Demo::SpaceshipGame::NativeScripts
