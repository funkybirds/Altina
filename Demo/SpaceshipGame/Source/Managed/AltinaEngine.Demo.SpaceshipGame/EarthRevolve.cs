using AltinaEngine.Managed;

namespace AltinaEngine.Demo.SpaceshipGame;

public sealed class EarthRevolve : ScriptComponent
{
    public override void OnCreate()
    {
        TrySetWorldPosition(CelestialMotion.EarthPosition(0.0f));
    }

    public override void Tick(float dt)
    {
        _ = dt;
    }
}

