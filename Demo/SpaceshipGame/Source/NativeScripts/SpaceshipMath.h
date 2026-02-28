#pragma once

#include "Math/LinAlg/SpatialTransform.h"
#include "Math/Rotation.h"
#include "Math/Vector.h"
#include "Types/Aliases.h"

#include <cmath>

namespace AltinaEngine::Demo::SpaceshipGame::NativeScripts {
    namespace Math   = AltinaEngine::Core::Math;
    namespace LinAlg = AltinaEngine::Core::Math::LinAlg;

    [[nodiscard]] inline auto WrapAngleRad(f32 rad) noexcept -> f32 {
        // Wrap to [-pi, pi].
        constexpr f32 twoPi = 6.2831853f;
        rad                 = std::fmod(rad, twoPi);
        if (rad > 3.14159265f)
            rad -= twoPi;
        if (rad < -3.14159265f)
            rad += twoPi;
        return rad;
    }

    [[nodiscard]] inline auto Clamp(f32 v, f32 minV, f32 maxV) noexcept -> f32 {
        if (v < minV)
            return minV;
        if (v > maxV)
            return maxV;
        return v;
    }

    [[nodiscard]] inline auto LengthXZ(const Math::FVector3f& v) noexcept -> f32 {
        const f32 x = v.X();
        const f32 z = v.Z();
        return std::sqrt(x * x + z * z);
    }

    [[nodiscard]] inline auto NormalizeXZ(const Math::FVector3f& v) noexcept -> Math::FVector3f {
        const f32 len = LengthXZ(v);
        if (len <= 1e-6f) {
            return Math::FVector3f(1.0f, 0.0f, 0.0f);
        }
        const f32 inv = 1.0f / len;
        return Math::FVector3f(v.X() * inv, 0.0f, v.Z() * inv);
    }

    [[nodiscard]] inline auto PerpLeftXZ(const Math::FVector3f& axisX) noexcept -> Math::FVector3f {
        return Math::FVector3f(-axisX.Z(), 0.0f, axisX.X());
    }

    [[nodiscard]] inline auto RotateY(const Math::FVector3f& v, f32 yawRad) noexcept
        -> Math::FVector3f {
        const f32 s = std::sin(yawRad);
        const f32 c = std::cos(yawRad);
        return Math::FVector3f(v.X() * c + v.Z() * s, v.Y(), -v.X() * s + v.Z() * c);
    }

    [[nodiscard]] inline auto FromEuler(f32 pitchRad, f32 yawRad, f32 rollRad) noexcept
        -> Math::FQuaternion {
        const Math::FEulerRotator euler(pitchRad, yawRad, rollRad);
        return euler.ToQuaternion().Normalized();
    }
} // namespace AltinaEngine::Demo::SpaceshipGame::NativeScripts
