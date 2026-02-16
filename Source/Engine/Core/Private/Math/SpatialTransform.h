#pragma once

#include "Math/Matrix.h"
#include "Math/Quaternion.h"
#include "Math/Vector.h"

namespace AltinaEngine::Core::Math {

    class FSpatialTransform {
    public:
        FQuaternion Rotation    = FQuaternion::Identity();
        FVector3f   Translation = FVector3f(0.0f);
        FVector3f   Scale       = FVector3f(1.0f);

        constexpr FSpatialTransform() noexcept = default;

        constexpr FSpatialTransform(const FQuaternion& InRotation, const FVector3f& InTranslation,
            const FVector3f& InScale) noexcept
            : Rotation(InRotation), Translation(InTranslation), Scale(InScale) {}

        static constexpr FSpatialTransform Identity() noexcept {
            return FSpatialTransform(FQuaternion::Identity(), FVector3f(0.0f), FVector3f(1.0f));
        }

        [[nodiscard]] FMatrix4x4f ToMatrix() const noexcept {
            const FQuaternion q = Rotation.Normalized();

            const f32         xx = q.x * q.x;
            const f32         yy = q.y * q.y;
            const f32         zz = q.z * q.z;
            const f32         xy = q.x * q.y;
            const f32         xz = q.x * q.z;
            const f32         yz = q.y * q.z;
            const f32         wx = q.w * q.x;
            const f32         wy = q.w * q.y;
            const f32         wz = q.w * q.z;

            const f32         r00 = 1.0f - 2.0f * (yy + zz);
            const f32         r01 = 2.0f * (xy - wz);
            const f32         r02 = 2.0f * (xz + wy);

            const f32         r10 = 2.0f * (xy + wz);
            const f32         r11 = 1.0f - 2.0f * (xx + zz);
            const f32         r12 = 2.0f * (yz - wx);

            const f32         r20 = 2.0f * (xz - wy);
            const f32         r21 = 2.0f * (yz + wx);
            const f32         r22 = 1.0f - 2.0f * (xx + yy);

            const f32         sx = Scale.X();
            const f32         sy = Scale.Y();
            const f32         sz = Scale.Z();

            FMatrix4x4f       out(0.0f);
            out(0, 0) = r00 * sx;
            out(0, 1) = r01 * sy;
            out(0, 2) = r02 * sz;
            out(0, 3) = Translation.X();

            out(1, 0) = r10 * sx;
            out(1, 1) = r11 * sy;
            out(1, 2) = r12 * sz;
            out(1, 3) = Translation.Y();

            out(2, 0) = r20 * sx;
            out(2, 1) = r21 * sy;
            out(2, 2) = r22 * sz;
            out(2, 3) = Translation.Z();

            out(3, 3) = 1.0f;
            return out;
        }

        [[nodiscard]] FSpatialTransform operator*(const FSpatialTransform& other) const noexcept {
            FSpatialTransform result;
            result.Scale                      = Scale * other.Scale;
            result.Rotation                   = Rotation * other.Rotation;
            const FVector3f scaledTranslation = other.Translation * Scale;
            result.Translation = Rotation.RotateVector(scaledTranslation) + Translation;
            return result;
        }

        auto operator*=(const FSpatialTransform& other) noexcept -> FSpatialTransform& {
            *this = *this * other;
            return *this;
        }
    };

} // namespace AltinaEngine::Core::Math
