namespace AltinaEngine.Managed;

public abstract class ScriptComponent
{
    public ulong InstanceHandle { get; internal set; }
    public uint OwnerIndex { get; internal set; }
    public uint OwnerGeneration { get; internal set; }
    public uint WorldId { get; internal set; }

    protected unsafe bool TryGetWorldPosition(out Vector3 position)
    {
        position = Vector3.Zero;
        var fn = Native.Api.GetWorldTranslation;
        if (fn == null)
        {
            return false;
        }

        Vector3* buffer = stackalloc Vector3[1];
        if (!fn(WorldId, OwnerIndex, OwnerGeneration, buffer))
        {
            return false;
        }
        position = buffer[0];
        return true;
    }

    protected unsafe bool TrySetWorldPosition(Vector3 position)
    {
        var fn = Native.Api.SetWorldTranslation;
        if (fn == null)
        {
            return false;
        }

        Vector3* buffer = stackalloc Vector3[1];
        buffer[0] = position;
        return fn(WorldId, OwnerIndex, OwnerGeneration, buffer);
    }

    public virtual void OnCreate() { }
    public virtual void OnDestroy() { }
    public virtual void OnEnable() { }
    public virtual void OnDisable() { }
    public virtual void Tick(float dt) { }
}
