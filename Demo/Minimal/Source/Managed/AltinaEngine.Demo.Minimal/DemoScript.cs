using AltinaEngine.Managed;

namespace AltinaEngine.Demo.Minimal;

public sealed class DemoScript : ScriptComponent
{
    private float _elapsedSeconds;
    private bool _loggedCreate;
    private bool _loggedFirstTick;

    public override void OnCreate()
    {
        if (_loggedCreate)
        {
            return;
        }

        _loggedCreate = true;
        ManagedLog.Info($"[DemoScript] OnCreate owner=({OwnerIndex},{OwnerGeneration}) world={WorldId}");
    }

    public override void OnDestroy()
    {
        ManagedLog.Info("[DemoScript] OnDestroy");
    }

    public override void Tick(float dt)
    {
        if (!_loggedFirstTick)
        {
            _loggedFirstTick = true;
            ManagedLog.Info("[DemoScript] Tick start.");
        }

        _elapsedSeconds += dt;
        if (_elapsedSeconds >= 1.0f)
        {
            _elapsedSeconds = 0.0f;
            ManagedLog.Info($"[DemoScript] Tick mouse=({Input.MouseX},{Input.MouseY})");
        }

        if (Input.WasKeyPressed(EKey.Space))
        {
            ManagedLog.Info("[DemoScript] Space pressed (managed).");
        }
    }
}
