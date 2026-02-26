using System;
using AltinaEngine.Managed;

namespace AltinaEngine.Demo.SpaceshipGame;

public sealed class ShipCameraModes : ScriptComponent
{
    private enum EMode
    {
        FirstPerson = 0,
        ThirdPerson,
    }

    private EMode _mode = EMode.FirstPerson;

    private float _tpYawRad;
    private float _tpPitchRad;
    private float _tpDistance;

    private const float MouseSensitivity = 0.0025f;
    private const float PitchLimit = 1.25f; // ~71 deg

    private const float DistanceMin = 0.12f;
    private const float DistanceMax = 1.50f;
    private const float DistanceWheelSpeed = 0.12f;

    private static readonly Vector3 CockpitOffset = new(0.0f, 0.02f, 0.07f);
    private static readonly Vector3 ThirdPersonTargetOffset = new(0.0f, 0.02f, 0.0f);

    public override void OnCreate()
    {
        _tpYawRad = 0.0f;
        _tpPitchRad = 0.2f;
        _tpDistance = 0.35f;
        SetMode(EMode.FirstPerson);
    }

    public override void Tick(float dt)
    {
        _ = dt;

        if (Input.WasKeyPressed(EKey.C))
        {
            SetMode(_mode == EMode.FirstPerson ? EMode.ThirdPerson : EMode.FirstPerson);
        }

        if (_mode == EMode.ThirdPerson)
        {
            UpdateThirdPersonInput();
            ApplyThirdPerson();
        }
    }

    private void SetMode(EMode mode)
    {
        _mode = mode;
        SpaceshipGlobals.ThirdPersonCameraEnabled = (mode == EMode.ThirdPerson);

        if (mode == EMode.FirstPerson)
        {
            TrySetLocalPosition(CockpitOffset);
            TrySetLocalRotation(Quaternion.Identity);
        }
        else
        {
            ApplyThirdPerson();
        }
    }

    private void UpdateThirdPersonInput()
    {
        if (!Input.HasFocus)
        {
            return;
        }

        int dx = Input.MouseDeltaX;
        int dy = Input.MouseDeltaY;

        _tpYawRad += dx * MouseSensitivity;
        _tpPitchRad += -dy * MouseSensitivity;
        _tpPitchRad = SpaceshipMath.Clamp(_tpPitchRad, -PitchLimit, PitchLimit);

        float wheel = Input.MouseWheelDelta;
        if (MathF.Abs(wheel) > 1e-5f)
        {
            _tpDistance -= wheel * DistanceWheelSpeed;
            _tpDistance = SpaceshipMath.Clamp(_tpDistance, DistanceMin, DistanceMax);
        }
    }

    private void ApplyThirdPerson()
    {
        // Orbit around the ship in local space (camera GameObject is parented to the ship).
        Vector3 backward = new(0.0f, 0.0f, -_tpDistance);
        Vector3 yawed = SpaceshipMath.RotateY(backward, _tpYawRad);

        // Pitch around local X. Since we only need orbit camera, apply pitch in the yawed frame.
        float sp = MathF.Sin(_tpPitchRad);
        float cp = MathF.Cos(_tpPitchRad);
        Vector3 pitched = new(yawed.X, yawed.Y * cp - yawed.Z * sp, yawed.Y * sp + yawed.Z * cp);

        Vector3 localPos = SpaceshipMath.Add(ThirdPersonTargetOffset, pitched);
        TrySetLocalPosition(localPos);

        // Look at the ship (target is at origin + offset).
        Vector3 toTarget = SpaceshipMath.Sub(ThirdPersonTargetOffset, localPos);
        Quaternion q = LookRotationFromForward(toTarget);
        TrySetLocalRotation(q);
    }

    private static Quaternion LookRotationFromForward(in Vector3 forward)
    {
        // Convert forward direction into Euler (yaw, pitch) and build quaternion.
        float lenSq = forward.X * forward.X + forward.Y * forward.Y + forward.Z * forward.Z;
        if (lenSq <= 1e-8f)
        {
            return Quaternion.Identity;
        }

        float invLen = 1.0f / MathF.Sqrt(lenSq);
        Vector3 f = new(forward.X * invLen, forward.Y * invLen, forward.Z * invLen);

        float yaw = MathF.Atan2(f.X, f.Z);
        float xz = MathF.Sqrt(f.X * f.X + f.Z * f.Z);
        float pitch = MathF.Atan2(f.Y, xz);

        return SpaceshipMath.FromEuler(pitch, yaw, 0.0f);
    }
}
