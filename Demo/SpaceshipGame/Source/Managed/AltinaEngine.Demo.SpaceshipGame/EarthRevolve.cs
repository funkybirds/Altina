using AltinaEngine.Managed;

namespace AltinaEngine.Demo.SpaceshipGame;

public sealed class EarthRevolve : ScriptComponent
{
    private float _t;

    public override void Tick(float dt)
    {
        _t += dt;
        TrySetWorldPosition(CelestialMotion.EarthPosition(_t));
    }
}

