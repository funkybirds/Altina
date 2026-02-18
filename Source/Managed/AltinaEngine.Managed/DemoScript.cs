namespace AltinaEngine.Managed;

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
        Native.LogInfo($"[DemoScript] OnCreate owner=({OwnerIndex},{OwnerGeneration}) world={WorldId}");
    }

    public override void OnDestroy()
    {
        Native.LogInfo("[DemoScript] OnDestroy");
    }

    public override void Tick(float dt)
    {
        if (!_loggedFirstTick)
        {
            _loggedFirstTick = true;
            Native.LogInfo("[DemoScript] Tick start.");
        }

        _elapsedSeconds += dt;
        if (_elapsedSeconds >= 1.0f)
        {
            _elapsedSeconds = 0.0f;
            Native.LogInfo($"[DemoScript] Tick mouse=({Input.MouseX},{Input.MouseY})");
        }

        if (Input.WasKeyPressed(EKey.Space))
        {
            Native.LogInfo("[DemoScript] Space pressed (managed).");
        }
    }
}
