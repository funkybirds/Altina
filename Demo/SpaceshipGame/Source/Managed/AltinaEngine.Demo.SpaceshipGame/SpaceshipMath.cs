using System;
using AltinaEngine.Managed;

namespace AltinaEngine.Demo.SpaceshipGame;

internal static class SpaceshipMath
{
    public static float WrapAngleRad(float rad)
    {
        // Wrap to [-PI, PI].
        const float twoPi = 6.2831853f;
        rad %= twoPi;
        if (rad > MathF.PI) rad -= twoPi;
        if (rad < -MathF.PI) rad += twoPi;
        return rad;
    }

    public static float Clamp(float v, float min, float max)
    {
        if (v < min) return min;
        if (v > max) return max;
        return v;
    }

    public static Vector3 Add(in Vector3 a, in Vector3 b) => new(a.X + b.X, a.Y + b.Y, a.Z + b.Z);
    public static Vector3 Sub(in Vector3 a, in Vector3 b) => new(a.X - b.X, a.Y - b.Y, a.Z - b.Z);
    public static Vector3 Mul(in Vector3 v, float s) => new(v.X * s, v.Y * s, v.Z * s);

    public static float DotXZ(in Vector3 a, in Vector3 b) => a.X * b.X + a.Z * b.Z;

    public static float LengthXZ(in Vector3 v) => MathF.Sqrt(v.X * v.X + v.Z * v.Z);

    public static Vector3 NormalizeXZ(in Vector3 v)
    {
        float len = LengthXZ(v);
        if (len <= 1e-6f) return new Vector3(1.0f, 0.0f, 0.0f);
        float inv = 1.0f / len;
        return new Vector3(v.X * inv, 0.0f, v.Z * inv);
    }

    public static Vector3 RotateY(in Vector3 v, float yawRad)
    {
        float s = MathF.Sin(yawRad);
        float c = MathF.Cos(yawRad);
        return new Vector3(v.X * c + v.Z * s, v.Y, -v.X * s + v.Z * c);
    }

    // XZ-plane "perp" axis (90deg to the left around +Y).
    public static Vector3 PerpLeftXZ(in Vector3 axisX) => new(-axisX.Z, 0.0f, axisX.X);

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
