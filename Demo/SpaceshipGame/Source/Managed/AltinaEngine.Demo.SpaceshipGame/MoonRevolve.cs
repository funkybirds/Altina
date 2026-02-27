using AltinaEngine.Managed;

namespace AltinaEngine.Demo.SpaceshipGame;

public sealed class MoonRevolve : ScriptComponent
{
    public override void OnCreate()
    {
        TrySetWorldPosition(CelestialMotion.MoonPosition(0.0f));
    }

    public override void Tick(float dt)
    {
        _ = dt;
    }
}

