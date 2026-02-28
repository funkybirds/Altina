#pragma once

#include "Math/Vector.h"
#include "SpaceshipConstants.h"

namespace AltinaEngine::Demo::SpaceshipGame::NativeScripts {
    namespace Math = AltinaEngine::Core::Math;

    struct FCelestialMotion final {
        static auto SunPosition([[maybe_unused]] f32 t) noexcept -> Math::FVector3f {
            // Keep consistent with demo scene setup in Demo/SpaceshipGame/Source/Main.cpp.
            return Math::FVector3f(400.0f, 0.0f, -400.0f);
        }

        static auto EarthPosition([[maybe_unused]] f32 t) noexcept -> Math::FVector3f {
            // Static bodies for v1 prototype.
            return Math::FVector3f(FSpaceshipConstants::SunEarthDistance, 0.0f, 0.0f);
        }

        static auto MoonPosition([[maybe_unused]] f32 t) noexcept -> Math::FVector3f {
            // Earth is static; use t=0 consistently.
            const Math::FVector3f earth = EarthPosition(0.0f);
            return Math::FVector3f(
                earth.X() + FSpaceshipConstants::EarthMoonDistance, 0.0f, earth.Z());
        }
    };
} // namespace AltinaEngine::Demo::SpaceshipGame::NativeScripts
