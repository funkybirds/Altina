using AltinaEngine.Managed;

namespace AltinaEngine.Demo.SpaceshipGame;

public sealed class FpsCameraFollow : ScriptComponent
{
    private static readonly Vector3 CockpitOffset = new(0.0f, 0.10f, 0.08f);

    public override void OnCreate()
    {
        TrySetLocalPosition(CockpitOffset);
        TrySetLocalRotation(Quaternion.Identity);
    }

    public override void Tick(float dt)
    {
        _ = dt;
        // Orientation is driven by the ship (camera is parented to the ship).
    }
}
