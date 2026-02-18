using System.Runtime.InteropServices;

namespace AltinaEngine.Managed;

[StructLayout(LayoutKind.Sequential)]
public unsafe struct ManagedCreateArgs
{
    public byte* AssemblyPathUtf8;
    public byte* TypeNameUtf8;
    public uint OwnerIndex;
    public uint OwnerGeneration;
    public uint WorldId;
}

[StructLayout(LayoutKind.Sequential)]
public unsafe struct ManagedApi
{
    public delegate* unmanaged[Cdecl]<ManagedCreateArgs*, ulong> CreateInstance;
    public delegate* unmanaged[Cdecl]<ulong, void> DestroyInstance;
    public delegate* unmanaged[Cdecl]<ulong, void> OnCreate;
    public delegate* unmanaged[Cdecl]<ulong, void> OnDestroy;
    public delegate* unmanaged[Cdecl]<ulong, void> OnEnable;
    public delegate* unmanaged[Cdecl]<ulong, void> OnDisable;
    public delegate* unmanaged[Cdecl]<ulong, float, void> Tick;
}
