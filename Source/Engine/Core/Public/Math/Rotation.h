#pragma once

#include "Types/Aliases.h"
#include "Common.h"
#include "Quaternion.h"
#include <cmath>

namespace AltinaEngine::Core::Math {

    // Euler rotation (pitch, yaw, roll) in radians.
    // Conversion uses yaw (Y) -> pitch (X) -> roll (Z) order.
    class FEulerRotator {
    public:
        f32 pitch = 0.0f;
        f32 yaw   = 0.0f;
        f32 roll  = 0.0f;

        constexpr FEulerRotator() noexcept = default;

        constexpr FEulerRotator(f32 InPitch, f32 InYaw, f32 InRoll) noexcept
            : pitch(InPitch), yaw(InYaw), roll(InRoll) {}

        static constexpr FEulerRotator Identity() noexcept {
            return FEulerRotator(0.0f, 0.0f, 0.0f);
        }

        explicit FEulerRotator(const FQuaternion& quat) noexcept
            : FEulerRotator(FromQuaternion(quat)) {}

        [[nodiscard]] FQuaternion ToQuaternion() const noexcept {
            const f32 halfPitch = pitch * 0.5f;
            const f32 halfYaw   = yaw * 0.5f;
            const f32 halfRoll  = roll * 0.5f;

            const f32 sx = Sin(halfPitch);
            const f32 cx = Cos(halfPitch);
            const f32 sy = Sin(halfYaw);
            const f32 cy = Cos(halfYaw);
            const f32 sz = Sin(halfRoll);
            const f32 cz = Cos(halfRoll);

            return FQuaternion(cy * sx * cz + sy * cx * sz, sy * cx * cz - cy * sx * sz,
                cy * cx * sz - sy * sx * cz, cy * cx * cz + sy * sx * sz);
        }

        static FEulerRotator FromQuaternion(const FQuaternion& quat) noexcept {
            const FQuaternion q = quat.Normalized();

            f32               sinPitch = 2.0f * (q.w * q.x - q.y * q.z);
            if (sinPitch > 1.0f) {
                sinPitch = 1.0f;
            } else if (sinPitch < -1.0f) {
                sinPitch = -1.0f;
            }

            const f32 absSinPitch = std::fabs(sinPitch);
            if (absSinPitch >= 0.999999f) {
                const f32 pitch = (sinPitch >= 0.0f) ? kHalfPiF : -kHalfPiF;
                const f32 r11   = 1.0f - 2.0f * (q.y * q.y + q.z * q.z);
                const f32 r12   = 2.0f * (q.x * q.y - q.z * q.w);
                const f32 yaw   = (sinPitch >= 0.0f) ? Atan2(r12, r11) : Atan2(-r12, r11);
                const f32 roll  = 0.0f;
                return FEulerRotator(pitch, yaw, roll);
            }

            const f32 pitch = Asin(sinPitch);
            const f32 yaw =
                Atan2(2.0f * (q.w * q.y + q.x * q.z), 1.0f - 2.0f * (q.x * q.x + q.y * q.y));
            const f32 roll =
                Atan2(2.0f * (q.w * q.z + q.x * q.y), 1.0f - 2.0f * (q.x * q.x + q.z * q.z));

            return FEulerRotator(pitch, yaw, roll);
        }
    };

} // namespace AltinaEngine::Core::Math
