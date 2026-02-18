using System;
using System.Runtime.InteropServices;

namespace AltinaEngine.Scripting.Tests;

[UnmanagedFunctionPointer(CallingConvention.Cdecl)]
public unsafe delegate int ManagedEntryPointDelegate(IntPtr args, int size);

public static unsafe class InteropEntry
{
    [StructLayout(LayoutKind.Sequential)]
    public struct InteropPayload
    {
        public delegate* unmanaged[Cdecl]<int, int, int> Callback;
        public int A;
        public int B;
        public int Result;
        public int CallbackHit;
    }

    [UnmanagedCallersOnly]
    public static int EntryPoint(IntPtr args, int size)
    {
        return ManagedEntryPoint(args, size);
    }

    public static int ManagedEntryPoint(IntPtr args, int size)
    {
        if (args == IntPtr.Zero)
        {
            return -1;
        }

        if (size < sizeof(InteropPayload))
        {
            return -2;
        }

        var payload = (InteropPayload*)args;
        if (payload->Callback == null)
        {
            return -3;
        }

        payload->Result = payload->Callback(payload->A, payload->B);
        payload->CallbackHit = 1;
        return 0;
    }
}
