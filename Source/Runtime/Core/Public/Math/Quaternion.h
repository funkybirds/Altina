#pragma once

#include "Types/Aliases.h"
#include "Common.h"
#include "Vector.h"

namespace AltinaEngine::Core::Math {

    // Simple quaternion type for 3D rotations. Public header only exposes
    // a minimal, well-tested API used by higher-level systems.
    class FQuaternion {
    public:
        f32 x = 0.0f;
        f32 y = 0.0f;
        f32 z = 0.0f;
        f32 w = 1.0f;

        constexpr FQuaternion() noexcept = default;

        constexpr FQuaternion(f32 InX, f32 InY, f32 InZ, f32 InW) noexcept
            : x(InX), y(InY), z(InZ), w(InW) {}

        static constexpr FQuaternion Identity() noexcept {
            return FQuaternion(0.0f, 0.0f, 0.0f, 1.0f);
        }

        // Create quaternion representing rotation of `angleRad` around `axis`.
        static FQuaternion FromAxisAngle(const FVector3f& axis, f32 angleRad) noexcept {
            // normalize axis
            FVector3f a   = axis;
            f32       len = Sqrt(a.X() * a.X() + a.Y() * a.Y() + a.Z() * a.Z());
            if (len <= 0.0f)
                return Identity();
            a.X() /= len;
            a.Y() /= len;
            a.Z() /= len;

            const f32 half = angleRad * 0.5f;
            const f32 s    = Sin(half);
            return FQuaternion(a.X() * s, a.Y() * s, a.Z() * s, Cos(half));
        }

        [[nodiscard]] f32 Length() const noexcept { return Sqrt(x * x + y * y + z * z + w * w); }

        [[nodiscard]] FQuaternion Normalized() const noexcept {
            const f32 l = Length();
            if (l <= 0.0f)
                return Identity();
            const f32 inv = 1.0f / l;
            return FQuaternion(x * inv, y * inv, z * inv, w * inv);
        }

        [[nodiscard]] FQuaternion Conjugate() const noexcept { return FQuaternion(-x, -y, -z, w); }

        [[nodiscard]] FQuaternion Inverse() const noexcept {
            const f32 norm2 = x * x + y * y + z * z + w * w;
            if (norm2 <= 0.0f)
                return Identity();
            const f32 inv = 1.0f / norm2;
            auto      c   = Conjugate();
            return FQuaternion(c.x * inv, c.y * inv, c.z * inv, c.w * inv);
        }

        // Quaternion multiplication (this * other)
        [[nodiscard]] FQuaternion operator*(const FQuaternion& other) const noexcept {
            return FQuaternion(w * other.x + x * other.w + y * other.z - z * other.y,
                w * other.y - x * other.z + y * other.w + z * other.x,
                w * other.z + x * other.y - y * other.x + z * other.w,
                w * other.w - x * other.x - y * other.y - z * other.z);
        }

        // Rotate a vector by this quaternion: q * (v as quaternion) * q^{-1}
        [[nodiscard]] FVector3f RotateVector(const FVector3f& v) const noexcept {
            // Convert vector to quaternion form (x,y,z,0)
            FQuaternion qv(v.X(), v.Y(), v.Z(), 0.0f);
            FQuaternion res = (*this) * qv * Inverse();
            return FVector3f(res.x, res.y, res.z);
        }
    };

} // namespace AltinaEngine::Core::Math
