using System;
using AltinaEngine.Managed;

namespace AltinaEngine.Demo.SpaceshipGame;

internal static class SpaceshipMath
{
    public static float Clamp(float v, float min, float max)
    {
        if (v < min) return min;
        if (v > max) return max;
        return v;
    }

    public static Quaternion Normalize(Quaternion q)
    {
        float len2 = q.X * q.X + q.Y * q.Y + q.Z * q.Z + q.W * q.W;
        if (len2 <= 1e-10f)
        {
            return Quaternion.Identity;
        }

        float invLen = 1.0f / MathF.Sqrt(len2);
        return new Quaternion(q.X * invLen, q.Y * invLen, q.Z * invLen, q.W * invLen);
    }

    // Match Core::Math::FEulerRotator::ToQuaternion (yaw(Y) -> pitch(X) -> roll(Z)).
    public static Quaternion FromEuler(float pitchRad, float yawRad, float rollRad)
    {
        float halfPitch = pitchRad * 0.5f;
        float halfYaw = yawRad * 0.5f;
        float halfRoll = rollRad * 0.5f;

        float sx = MathF.Sin(halfPitch);
        float cx = MathF.Cos(halfPitch);
        float sy = MathF.Sin(halfYaw);
        float cy = MathF.Cos(halfYaw);
        float sz = MathF.Sin(halfRoll);
        float cz = MathF.Cos(halfRoll);

        Quaternion q = new(
            cy * sx * cz + sy * cx * sz,
            sy * cx * cz - cy * sx * sz,
            cy * cx * sz - sy * sx * cz,
            cy * cx * cz + sy * sx * sz
        );

        return Normalize(q);
    }
}

