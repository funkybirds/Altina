using AltinaEngine.Managed;

namespace AltinaEngine.Demo.SpaceshipGame;

public sealed class FpsCameraController : ScriptComponent
{
    private float _yawRad;
    private float _pitchRad;

    private const float MouseSensitivity = 0.0025f;
    private const float PitchLimit = 1.35f; // ~77 deg

    private static readonly Vector3 CockpitOffset = new(0.0f, 2.0f, 0.0f);

    public override void OnCreate()
    {
        // Keep the camera slightly above the ship origin.
        TrySetLocalPosition(CockpitOffset);
        TrySetLocalRotation(Quaternion.Identity);
    }

    public override void Tick(float dt)
    {
        if (!Input.HasFocus)
        {
            return;
        }

        int dx = Input.MouseDeltaX;
        int dy = Input.MouseDeltaY;

        _yawRad += dx * MouseSensitivity;
        _pitchRad += -dy * MouseSensitivity;
        _pitchRad = SpaceshipMath.Clamp(_pitchRad, -PitchLimit, PitchLimit);

        Quaternion q = SpaceshipMath.FromEuler(_pitchRad, _yawRad, 0.0f);
        TrySetLocalRotation(q);
    }
}

