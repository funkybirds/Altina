namespace AltinaEngine.Managed;

public abstract class ScriptComponent
{
    public ulong InstanceHandle { get; internal set; }
    public uint OwnerIndex { get; internal set; }
    public uint OwnerGeneration { get; internal set; }
    public uint WorldId { get; internal set; }

    public virtual void OnCreate() { }
    public virtual void OnDestroy() { }
    public virtual void OnEnable() { }
    public virtual void OnDisable() { }
    public virtual void Tick(float dt) { }
}
