using System;
using AltinaEngine.Managed;

namespace AltinaEngine.Demo.SpaceshipGame;

public sealed class ShipOrbitController : ScriptComponent
{
    private enum EShipState
    {
        EarthOrbit = 0,
        Transfer,
        MoonOrbit,
    }

    private EShipState _state = EShipState.EarthOrbit;

    private float _t;
    private float _timeScale = 1.0f;

    private float _orbitPhaseRad;
    private float _transferThetaRad;
    private float _transferS01; // normalized arc-length progress on transfer [0,1)

    private bool _transferLutBuilt;
    private float[] _transferLutTheta = Array.Empty<float>();
    private float[] _transferLutS01 = Array.Empty<float>();
    private float _transferSAtTheta0;
    private float _transferSAtThetaPi;

    // Mouse-controlled offsets applied on top of the state-dependent base orientation (usually
    // aligned with the current movement direction). This keeps "look at X" stable while orbiting
    // without overriding user input each tick.
    private float _yawOffsetRad;
    private float _pitchOffsetRad;

    private bool _titleInitialized;
    private EShipState _lastTitleState;
    private bool _lastTitleCanSwitchEarth;
    private bool _lastTitleCanSwitchMoon;
    private string _lastWindowTitle = string.Empty;

    // Workaround: some GUI / focus integrations can cause "WasKeyPressed" to miss Space in
    // particular, while "IsKeyDown" still works. Keep a local edge detector so Space transfers
    // remain reliable.
    private bool _spaceWasDown;

    private const float MouseSensitivity = 0.0025f;
    private const float PitchLimit = 1.35f; // ~77 deg

    public override void OnCreate()
    {
        _state = EShipState.EarthOrbit;
        _timeScale = 1.0f;
        // Start at a phase where the ship's tangent (+forward) roughly faces the Moon direction.
        // This makes the Moon immediately visible in first-person mode without requiring mouse look.
        _orbitPhaseRad = -0.5f * MathF.PI;
        _transferThetaRad = 0.0f; // theta=0 => JoinEarth.
        _transferS01 = 0.0f;
        _yawOffsetRad = 0.0f;
        _pitchOffsetRad = 0.0f;

        _titleInitialized = false;
        _lastTitleState = _state;
        _lastTitleCanSwitchEarth = false;
        _lastTitleCanSwitchMoon = false;
        _lastWindowTitle = string.Empty;
        _spaceWasDown = false;

        // Bodies are static in this iteration; build the LUT once.
        BuildTransferLut(CelestialMotion.EarthPosition(0.0f), CelestialMotion.MoonPosition(0.0f));
        _transferS01 = _transferSAtTheta0;
    }

    public override void Tick(float dt)
    {
        _t += dt;

        UpdateTimeScale(dt);
        float scaledDt = dt * _timeScale;

        Vector3 earthPos = CelestialMotion.EarthPosition(_t);
        Vector3 moonPos = CelestialMotion.MoonPosition(_t);

        if (!_transferLutBuilt)
        {
            BuildTransferLut(earthPos, moonPos);
        }

        // Gate switches at the current position (before advancing this frame), then advance the
        // active orbit so switching feels responsive near join points.
        Vector3 shipPosBeforeAdvance = EvalShipPosition(earthPos, moonPos);

        bool canSwitchEarth = CanSwitchAtJoinEarth(earthPos, moonPos, shipPosBeforeAdvance);
        bool canSwitchMoon = CanSwitchAtJoinMoon(earthPos, moonPos, shipPosBeforeAdvance);
        UpdateWindowTitle(canSwitchEarth, canSwitchMoon);

        if (HandleStateSwitchInput(earthPos, moonPos, shipPosBeforeAdvance, canSwitchEarth, canSwitchMoon))
        {
            shipPosBeforeAdvance = EvalShipPosition(earthPos, moonPos);
        }

        AdvanceOrbit(scaledDt);
        Vector3 shipPos = EvalShipPosition(earthPos, moonPos);

        TrySetWorldPosition(shipPos);

        UpdateMouseOffsets();
        ApplyOrientation(dt, earthPos, moonPos);
    }

    private void UpdateWindowTitle(bool canSwitchEarth, bool canSwitchMoon)
    {
        bool changed = !_titleInitialized
            || _lastTitleState != _state
            || _lastTitleCanSwitchEarth != canSwitchEarth
            || _lastTitleCanSwitchMoon != canSwitchMoon;

        if (!changed)
        {
            return;
        }

        _titleInitialized = true;
        _lastTitleState = _state;
        _lastTitleCanSwitchEarth = canSwitchEarth;
        _lastTitleCanSwitchMoon = canSwitchMoon;

        // Keep ASCII-only to avoid encoding edge cases in early engine UI-less builds.
        // Primary UX: show when Space is usable for transfers.
        string switchText = string.Empty;
        switch (_state)
        {
            case EShipState.EarthOrbit:
                if (canSwitchEarth) switchText = "Hint: Space->Transfer";
                break;
            case EShipState.Transfer:
                if (canSwitchEarth && canSwitchMoon) switchText = "Hint: Space->EarthOrbit / MoonOrbit";
                else if (canSwitchEarth) switchText = "Hint: Space->EarthOrbit";
                else if (canSwitchMoon) switchText = "Hint: Space->MoonOrbit";
                break;
            case EShipState.MoonOrbit:
                if (canSwitchMoon) switchText = "Hint: Space->Transfer";
                break;
        }

        string title = string.IsNullOrEmpty(switchText)
            ? $"SpaceshipGame | {_state}"
            : $"SpaceshipGame | {_state} | {switchText}";

        if (title == _lastWindowTitle)
        {
            return;
        }

        _lastWindowTitle = title;
        Window.SetTitle(title);
    }

    private void UpdateMouseOffsets()
    {
        if (SpaceshipGlobals.ThirdPersonCameraEnabled)
        {
            // In third-person mode the mouse is used for orbit camera controls.
            return;
        }

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

    private bool HandleStateSwitchInput(in Vector3 earthPos, in Vector3 moonPos, in Vector3 shipPos,
        bool canSwitchEarth, bool canSwitchMoon)
    {
        bool changed = false;

        // Primary UX: press Space to switch when near a join point.
        if (!Input.HasFocus)
        {
            _spaceWasDown = false;
        }
        bool spaceDown = Input.IsKeyDown(EKey.Space);
        bool spacePressed = spaceDown && !_spaceWasDown;
        _spaceWasDown = spaceDown;

        if (spacePressed)
        {
            switch (_state)
            {
                case EShipState.EarthOrbit:
                    if (canSwitchEarth)
                    {
                        _state = EShipState.Transfer;
                        _transferS01 = _transferSAtTheta0;
                        _transferThetaRad = 0.0f; // JoinEarth.
                        changed = true;
                    }
                    break;
                case EShipState.Transfer:
                    if (canSwitchEarth)
                    {
                        _state = EShipState.EarthOrbit;
                        _orbitPhaseRad = MathF.PI; // JoinEarth.
                        changed = true;
                    }
                    else if (canSwitchMoon)
                    {
                        _state = EShipState.MoonOrbit;
                        _orbitPhaseRad = MathF.PI; // JoinMoon.
                        changed = true;
                    }
                    break;
                case EShipState.MoonOrbit:
                    if (canSwitchMoon)
                    {
                        _state = EShipState.Transfer;
                        _transferS01 = _transferSAtThetaPi;
                        _transferThetaRad = MathF.PI; // JoinMoon.
                        changed = true;
                    }
                    break;
            }
        }

        // Orbit-only state machine:
        // - 2: EarthOrbit (only from Transfer at JoinEarth)
        // - 3: Transfer (only from EarthOrbit at JoinEarth, or from MoonOrbit at JoinMoon)
        // - 4: MoonOrbit (only from Transfer at JoinMoon)
        if (Input.WasKeyPressed(EKey.Num2))
        {
            if (_state == EShipState.Transfer && canSwitchEarth)
            {
                _state = EShipState.EarthOrbit;
                _orbitPhaseRad = MathF.PI; // JoinEarth.
                changed = true;
            }
        }

        if (Input.WasKeyPressed(EKey.Num3))
        {
            if (_state == EShipState.EarthOrbit && canSwitchEarth)
            {
                _state = EShipState.Transfer;
                _transferS01 = _transferSAtTheta0;
                _transferThetaRad = 0.0f; // JoinEarth.
                changed = true;
            }
            else if (_state == EShipState.MoonOrbit && canSwitchMoon)
            {
                _state = EShipState.Transfer;
                _transferS01 = _transferSAtThetaPi;
                _transferThetaRad = MathF.PI; // JoinMoon.
                changed = true;
            }
        }

        if (Input.WasKeyPressed(EKey.Num4))
        {
            if (_state == EShipState.Transfer && canSwitchMoon)
            {
                _state = EShipState.MoonOrbit;
                _orbitPhaseRad = MathF.PI; // JoinMoon (moon near-side point).
                changed = true;
            }
        }

        return changed;
    }

    private void AdvanceOrbit(float scaledDt)
    {
        switch (_state)
        {
            case EShipState.EarthOrbit:
            {
                _orbitPhaseRad += scaledDt * 0.9f;
                _orbitPhaseRad = SpaceshipMath.WrapAngleRad(_orbitPhaseRad);
                break;
            }

            case EShipState.MoonOrbit:
            {
                // Reverse direction so transfer capture at JoinMoon is tangent-continuous.
                _orbitPhaseRad -= scaledDt * 1.8f;
                _orbitPhaseRad = SpaceshipMath.WrapAngleRad(_orbitPhaseRad);
                break;
            }

            case EShipState.Transfer:
            default:
                // Advance at (approx) constant speed along the transfer curve by using an
                // arc-length LUT (instead of theta stepping, which is non-uniform).
                const float thetaDot = 0.35f;
                const float twoPi = 6.2831853f;
                float fracPerSec = thetaDot / twoPi;
                _transferS01 = Wrap01(_transferS01 + scaledDt * fracPerSec);
                _transferThetaRad = SpaceshipMath.WrapAngleRad(ThetaFromS01(_transferS01));
                break;
        }

        return;
    }

    private Vector3 EvalShipPosition(in Vector3 earthPos, in Vector3 moonPos)
    {
        Vector3 earthToMoon = SpaceshipMath.Sub(moonPos, earthPos);
        Vector3 axisX = SpaceshipMath.NormalizeXZ(earthToMoon);
        Vector3 axisZ = SpaceshipMath.PerpLeftXZ(axisX);

        switch (_state)
        {
            case EShipState.EarthOrbit:
            {
                float c = MathF.Cos(_orbitPhaseRad);
                float s = MathF.Sin(_orbitPhaseRad);
                Vector3 local = SpaceshipMath.Add(SpaceshipMath.Mul(axisX, SpaceshipConstants.EarthOrbitRadius * c),
                    SpaceshipMath.Mul(axisZ, SpaceshipConstants.EarthOrbitRadius * s));
                return SpaceshipMath.Add(earthPos, local);
            }
            case EShipState.MoonOrbit:
            {
                float c = MathF.Cos(_orbitPhaseRad);
                float s = MathF.Sin(_orbitPhaseRad);
                Vector3 local = SpaceshipMath.Add(SpaceshipMath.Mul(axisX, SpaceshipConstants.MoonOrbitRadius * c),
                    SpaceshipMath.Mul(axisZ, SpaceshipConstants.MoonOrbitRadius * s));
                return SpaceshipMath.Add(moonPos, local);
            }
            case EShipState.Transfer:
            {
                const float r1 = SpaceshipConstants.EarthOrbitRadius;
                float r2 = SpaceshipConstants.EarthMoonDistance - SpaceshipConstants.MoonOrbitRadius;
                float a = 0.5f * (r1 + r2);
                float e = (r2 - r1) / (r2 + r1);
                return EvalTransferPosition(earthPos, axisX, axisZ, _transferThetaRad, a, e);
            }
            default:
                return earthPos;
        }
    }

    private void ApplyOrientation(float dt, in Vector3 earthPos, in Vector3 moonPos)
    {
        // Base yaw aims along the current movement direction (tangent).
        float baseYawRad = 0.0f;

        Vector3 earthToMoon = SpaceshipMath.Sub(moonPos, earthPos);
        Vector3 axisX = SpaceshipMath.NormalizeXZ(earthToMoon);
        Vector3 axisZ = SpaceshipMath.PerpLeftXZ(axisX);

        switch (_state)
        {
            case EShipState.EarthOrbit:
            {
                Vector3 t = EvalCircleTangent(axisX, axisZ, _orbitPhaseRad, +1.0f);
                baseYawRad = MathF.Atan2(t.X, t.Z);
                break;
            }
            case EShipState.MoonOrbit:
            {
                // Moon orbit runs phase in reverse (see AdvanceOrbit).
                Vector3 t = EvalCircleTangent(axisX, axisZ, _orbitPhaseRad, -1.0f);
                baseYawRad = MathF.Atan2(t.X, t.Z);
                break;
            }
            case EShipState.Transfer:
            {
                // Finite difference on the transfer orbit for tangent direction.
                float scaledDt = dt * _timeScale;

                const float r1 = SpaceshipConstants.EarthOrbitRadius;
                float r2 = SpaceshipConstants.EarthMoonDistance - SpaceshipConstants.MoonOrbitRadius;
                float a = 0.5f * (r1 + r2);
                float e = (r2 - r1) / (r2 + r1);

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

    private static Vector3 EvalCircleTangent(in Vector3 axisX, in Vector3 axisZ, float phase, float dirSign)
    {
        // d/dphase of (axisX*cos + axisZ*sin) = (-axisX*sin + axisZ*cos).
        float s = MathF.Sin(phase);
        float c = MathF.Cos(phase);
        Vector3 t = SpaceshipMath.Add(SpaceshipMath.Mul(axisX, -s), SpaceshipMath.Mul(axisZ, c));
        return SpaceshipMath.Mul(t, dirSign);
    }

    private static float DistXZ(in Vector3 a, in Vector3 b)
    {
        Vector3 d = SpaceshipMath.Sub(a, b);
        return MathF.Sqrt(d.X * d.X + d.Z * d.Z);
    }

    private static float Wrap01(float x)
    {
        x %= 1.0f;
        if (x < 0.0f) x += 1.0f;
        return x;
    }

    private void BuildTransferLut(in Vector3 earthPos, in Vector3 moonPos)
    {
        // LUT over theta in [0, 2PI], mapping normalized arc-length fraction -> theta.
        // This makes motion along the ellipse look close to constant-speed.
        Vector3 axisX = AxisX(earthPos, moonPos);
        Vector3 axisZ = SpaceshipMath.PerpLeftXZ(axisX);

        const float r1 = SpaceshipConstants.EarthOrbitRadius;
        float r2 = SpaceshipConstants.EarthMoonDistance - SpaceshipConstants.MoonOrbitRadius;
        float a = 0.5f * (r1 + r2);
        float e = (r2 - r1) / (r2 + r1);

        const int n = 4096;
        const float twoPi = 6.2831853f;

        _transferLutTheta = new float[n + 1];
        _transferLutS01 = new float[n + 1];

        Vector3 prev = EvalTransferPosition(earthPos, axisX, axisZ, 0.0f, a, e);
        _transferLutTheta[0] = 0.0f;
        _transferLutS01[0] = 0.0f;

        float total = 0.0f;
        for (int i = 1; i <= n; ++i)
        {
            float theta = twoPi * (i / (float)n);
            Vector3 p = EvalTransferPosition(earthPos, axisX, axisZ, theta, a, e);
            total += DistXZ(p, prev);
            _transferLutTheta[i] = theta;
            _transferLutS01[i] = total;
            prev = p;
        }

        if (total > 1e-6f)
        {
            for (int i = 1; i <= n; ++i)
            {
                _transferLutS01[i] /= total;
            }
        }

        _transferSAtTheta0 = 0.0f;
        _transferSAtThetaPi = _transferLutS01[n / 2];
        _transferLutBuilt = true;
    }

    private float ThetaFromS01(float s01)
    {
        if (_transferLutS01.Length == 0)
        {
            return 0.0f;
        }

        s01 = Wrap01(s01);

        // Binary search for first i with s[i] >= s01.
        int lo = 0;
        int hi = _transferLutS01.Length - 1;
        while (lo < hi)
        {
            int mid = (lo + hi) / 2;
            if (_transferLutS01[mid] < s01) lo = mid + 1;
            else hi = mid;
        }

        int i1 = lo;
        if (i1 <= 0) return _transferLutTheta[0];
        if (i1 >= _transferLutS01.Length) return _transferLutTheta[_transferLutTheta.Length - 1];

        int i0 = i1 - 1;
        float s0 = _transferLutS01[i0];
        float s1 = _transferLutS01[i1];
        float t0 = _transferLutTheta[i0];
        float t1 = _transferLutTheta[i1];
        float denom = s1 - s0;
        if (denom <= 1e-8f) return t1;

        float a = (s01 - s0) / denom;
        return t0 + (t1 - t0) * a;
    }

    private static Vector3 AxisX(in Vector3 earthPos, in Vector3 moonPos)
    {
        return SpaceshipMath.NormalizeXZ(SpaceshipMath.Sub(moonPos, earthPos));
    }

    private static Vector3 JoinEarth(in Vector3 earthPos, in Vector3 axisX)
    {
        return SpaceshipMath.Add(earthPos, SpaceshipMath.Mul(axisX, -SpaceshipConstants.EarthOrbitRadius));
    }

    private static Vector3 JoinMoon(in Vector3 moonPos, in Vector3 axisX)
    {
        return SpaceshipMath.Add(moonPos, SpaceshipMath.Mul(axisX, -SpaceshipConstants.MoonOrbitRadius));
    }

    private static bool CanSwitchAtJoinEarth(in Vector3 earthPos, in Vector3 moonPos, in Vector3 shipPos)
    {
        Vector3 axisX = AxisX(earthPos, moonPos);
        Vector3 join = JoinEarth(earthPos, axisX);
        return DistXZ(shipPos, join) <= SpaceshipConstants.OrbitSwitchEpsilon;
    }

    private static bool CanSwitchAtJoinMoon(in Vector3 earthPos, in Vector3 moonPos, in Vector3 shipPos)
    {
        Vector3 axisX = AxisX(earthPos, moonPos);
        Vector3 join = JoinMoon(moonPos, axisX);
        return DistXZ(shipPos, join) <= SpaceshipConstants.OrbitSwitchEpsilon;
    }
}
