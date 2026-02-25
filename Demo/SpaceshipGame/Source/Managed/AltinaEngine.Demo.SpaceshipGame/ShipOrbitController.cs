using System;
using AltinaEngine.Managed;

namespace AltinaEngine.Demo.SpaceshipGame;

public sealed class ShipOrbitController : ScriptComponent
{
    private enum EShipState
    {
        LandedEarth = 0,
        EarthOrbit,
        Transfer,
        MoonOrbit,
        LandedMoon,
    }

    private EShipState _state = EShipState.EarthOrbit;

    private float _t;
    private float _timeScale = 1.0f;

    private float _orbitPhaseRad;
    private float _transferThetaRad;

    // Mouse-controlled offsets applied on top of the state-dependent base orientation (usually
    // aligned with the current movement direction). This keeps "look at X" stable while orbiting
    // without overriding user input each tick.
    private float _yawOffsetRad;
    private float _pitchOffsetRad;

    private const float MouseSensitivity = 0.0025f;
    private const float PitchLimit = 1.35f; // ~77 deg

    public override void OnCreate()
    {
        _state = EShipState.EarthOrbit;
        _timeScale = 1.0f;
        _orbitPhaseRad = 0.0f;
        _transferThetaRad = 0.0f;
        _yawOffsetRad = 0.0f;
        _pitchOffsetRad = 0.0f;
    }

    public override void Tick(float dt)
    {
        _t += dt;

        UpdateTimeScale(dt);
        HandleStateSwitchInput();

        Vector3 shipPos = ComputeShipPosition(dt);
        TrySetWorldPosition(shipPos);

        UpdateMouseOffsets();
        ApplyOrientation(dt);
    }

    private void UpdateMouseOffsets()
    {
        if (!Input.HasFocus)
        {
            return;
        }

        int dx = Input.MouseDeltaX;
        int dy = Input.MouseDeltaY;

        _yawOffsetRad += dx * MouseSensitivity;
        _pitchOffsetRad += -dy * MouseSensitivity;
        _pitchOffsetRad = SpaceshipMath.Clamp(_pitchOffsetRad, -PitchLimit, PitchLimit);
    }

    private void UpdateTimeScale(float dt)
    {
        // Q increases time scale, E decreases time scale (orbit-only control).
        const float accel = 0.8f;
        if (Input.IsKeyDown(EKey.Q))
        {
            _timeScale += accel * dt;
        }
        if (Input.IsKeyDown(EKey.E))
        {
            _timeScale -= accel * dt;
        }

        _timeScale = SpaceshipMath.Clamp(_timeScale, 0.1f, 6.0f);
    }

    private void HandleStateSwitchInput()
    {
        if (Input.WasKeyPressed(EKey.Num1)) _state = EShipState.LandedEarth;
        if (Input.WasKeyPressed(EKey.Num5)) _state = EShipState.LandedMoon;

        if (Input.WasKeyPressed(EKey.Num3) && _state != EShipState.Transfer)
        {
            _state = EShipState.Transfer;
            _transferThetaRad = 0.0f; // periapsis
        }

        if (Input.WasKeyPressed(EKey.Num2))
        {
            if (_state == EShipState.Transfer) TrySwitchFromTransferToEarthOrbit();
            else _state = EShipState.EarthOrbit;
        }

        if (Input.WasKeyPressed(EKey.Num4))
        {
            if (_state == EShipState.Transfer) TrySwitchFromTransferToMoonOrbit();
            else _state = EShipState.MoonOrbit;
        }
    }

    private Vector3 ComputeShipPosition(float dt)
    {
        // Keep orbits readable regardless of delta time.
        float scaledDt = dt * _timeScale;

        Vector3 earthPos = CelestialMotion.EarthPosition(_t);
        Vector3 moonPos = CelestialMotion.MoonPosition(_t);

        switch (_state)
        {
            case EShipState.LandedEarth:
                // Land at "north pole" direction.
                return SpaceshipMath.Add(earthPos,
                    new Vector3(0.0f, SpaceshipConstants.EarthRadius + SpaceshipConstants.ShipRadius, 0.0f));

            case EShipState.EarthOrbit:
            {
                _orbitPhaseRad += scaledDt * 0.9f;
                Vector3 offset = new(SpaceshipConstants.EarthOrbitRadius, 0.0f, 0.0f);
                Vector3 local = SpaceshipMath.RotateY(offset, _orbitPhaseRad);
                return SpaceshipMath.Add(earthPos, local);
            }

            case EShipState.MoonOrbit:
            {
                _orbitPhaseRad += scaledDt * 1.8f;
                Vector3 offset = new(SpaceshipConstants.MoonOrbitRadius, 0.0f, 0.0f);
                Vector3 local = SpaceshipMath.RotateY(offset, _orbitPhaseRad);
                return SpaceshipMath.Add(moonPos, local);
            }

            case EShipState.LandedMoon:
                return SpaceshipMath.Add(moonPos,
                    new Vector3(0.0f, SpaceshipConstants.MoonRadius + SpaceshipConstants.ShipRadius, 0.0f));

            case EShipState.Transfer:
            default:
                return ComputeTransferPosition(earthPos, moonPos, scaledDt);
        }
    }

    private void ApplyOrientation(float dt)
    {
        // Base yaw aims along the current movement direction (tangent).
        float baseYawRad = 0.0f;

        switch (_state)
        {
            case EShipState.EarthOrbit:
            {
                // Derivative of RotateY((R,0,0), phase): (-sin, 0, -cos) (up to scale).
                float s = MathF.Sin(_orbitPhaseRad);
                float c = MathF.Cos(_orbitPhaseRad);
                baseYawRad = MathF.Atan2(-s, -c);
                break;
            }
            case EShipState.MoonOrbit:
            {
                float s = MathF.Sin(_orbitPhaseRad);
                float c = MathF.Cos(_orbitPhaseRad);
                baseYawRad = MathF.Atan2(-s, -c);
                break;
            }
            case EShipState.Transfer:
            {
                // Finite difference on the transfer orbit for tangent direction.
                float scaledDt = dt * _timeScale;

                Vector3 earthPos = CelestialMotion.EarthPosition(_t);
                Vector3 moonPos = CelestialMotion.MoonPosition(_t);
                Vector3 earthToMoon = SpaceshipMath.Sub(moonPos, earthPos);
                Vector3 axisX = SpaceshipMath.NormalizeXZ(earthToMoon);
                Vector3 axisZ = SpaceshipMath.PerpLeftXZ(axisX);

                const float r1 = SpaceshipConstants.EarthOrbitRadius;
                const float r2 = SpaceshipConstants.EarthMoonDistance;
                float a = 0.5f * (r1 + r2);
                float e = (r2 - r1) / (r2 + r1);

                // Use a small delta theta based on time step.
                float dTheta = MathF.Max(0.001f, MathF.Abs(scaledDt) * 0.02f);
                Vector3 p0 = EvalTransferPosition(earthPos, axisX, axisZ, _transferThetaRad, a, e);
                Vector3 p1 = EvalTransferPosition(earthPos, axisX, axisZ, _transferThetaRad + dTheta, a, e);
                Vector3 d = SpaceshipMath.Sub(p1, p0);
                float len = SpaceshipMath.LengthXZ(d);
                if (len > 1e-6f)
                {
                    baseYawRad = MathF.Atan2(d.X, d.Z);
                }
                break;
            }
            default:
                break;
        }

        float yaw = baseYawRad + _yawOffsetRad;
        Quaternion q = SpaceshipMath.FromEuler(_pitchOffsetRad, yaw, 0.0f);
        TrySetWorldRotation(q);
    }

    private Vector3 ComputeTransferPosition(in Vector3 earthPos, in Vector3 moonPos, float scaledDt)
    {
        // Rotate the ellipse basis with the current Earth->Moon direction.
        Vector3 earthToMoon = SpaceshipMath.Sub(moonPos, earthPos);
        Vector3 axisX = SpaceshipMath.NormalizeXZ(earthToMoon);
        Vector3 axisZ = SpaceshipMath.PerpLeftXZ(axisX);

        const float r1 = SpaceshipConstants.EarthOrbitRadius;
        const float r2 = SpaceshipConstants.EarthMoonDistance;
        float a = 0.5f * (r1 + r2);
        float e = (r2 - r1) / (r2 + r1);

        // Advance along the parametric ellipse.
        _transferThetaRad += scaledDt * 0.35f;
        _transferThetaRad = SpaceshipMath.WrapAngleRad(_transferThetaRad);

        return EvalTransferPosition(earthPos, axisX, axisZ, _transferThetaRad, a, e);
    }

    private static Vector3 EvalTransferPosition(in Vector3 earthPos, in Vector3 axisX,
        in Vector3 axisZ, float theta, float a, float e)
    {
        float p = a * (1.0f - e * e);
        float r = p / (1.0f + e * MathF.Cos(theta));

        // Offset by PI so theta=0 is periapsis (opposite moon direction) and theta=PI is apoapsis (toward moon).
        float phi = theta + MathF.PI;

        float c = MathF.Cos(phi);
        float s = MathF.Sin(phi);
        return SpaceshipMath.Add(earthPos,
            SpaceshipMath.Add(SpaceshipMath.Mul(axisX, r * c), SpaceshipMath.Mul(axisZ, r * s)));
    }

    private void TrySwitchFromTransferToEarthOrbit()
    {
        // Allow capture near periapsis: theta ~ 0.
        float d = MathF.Abs(SpaceshipMath.WrapAngleRad(_transferThetaRad));
        if (d <= SpaceshipConstants.TransferWindowHalfAngleRad)
        {
            _state = EShipState.EarthOrbit;
        }
    }

    private void TrySwitchFromTransferToMoonOrbit()
    {
        // Allow capture near apoapsis: theta ~ PI and ship close to the Moon.
        float d = MathF.Abs(SpaceshipMath.WrapAngleRad(_transferThetaRad - MathF.PI));
        if (d > SpaceshipConstants.TransferWindowHalfAngleRad)
        {
            return;
        }

        Vector3 earthPos = CelestialMotion.EarthPosition(_t);
        Vector3 moonPos = CelestialMotion.MoonPosition(_t);

        Vector3 earthToMoon = SpaceshipMath.Sub(moonPos, earthPos);
        Vector3 axisX = SpaceshipMath.NormalizeXZ(earthToMoon);
        Vector3 axisZ = SpaceshipMath.PerpLeftXZ(axisX);

        const float r1 = SpaceshipConstants.EarthOrbitRadius;
        const float r2 = SpaceshipConstants.EarthMoonDistance;
        float a = 0.5f * (r1 + r2);
        float e = (r2 - r1) / (r2 + r1);

        Vector3 shipPos = EvalTransferPosition(earthPos, axisX, axisZ, _transferThetaRad, a, e);

        Vector3 toMoon = SpaceshipMath.Sub(shipPos, moonPos);
        float dist = MathF.Sqrt(toMoon.X * toMoon.X + toMoon.Y * toMoon.Y + toMoon.Z * toMoon.Z);
        if (dist <= SpaceshipConstants.TransferMoonCaptureDistance)
        {
            _state = EShipState.MoonOrbit;
        }
    }
}
