using System;
using System.Runtime.InteropServices;
using System.Text;

namespace AltinaEngine.Managed;

[StructLayout(LayoutKind.Sequential)]
public unsafe struct NativeApi
{
    public delegate* unmanaged[Cdecl]<byte*, void> LogInfo;
    public delegate* unmanaged[Cdecl]<byte*, void> LogError;

    public delegate* unmanaged[Cdecl]<ushort, bool> IsKeyDown;
    public delegate* unmanaged[Cdecl]<ushort, bool> WasKeyPressed;
    public delegate* unmanaged[Cdecl]<ushort, bool> WasKeyReleased;

    public delegate* unmanaged[Cdecl]<uint, bool> IsMouseButtonDown;
    public delegate* unmanaged[Cdecl]<uint, bool> WasMouseButtonPressed;
    public delegate* unmanaged[Cdecl]<uint, bool> WasMouseButtonReleased;

    public delegate* unmanaged[Cdecl]<int> GetMouseX;
    public delegate* unmanaged[Cdecl]<int> GetMouseY;
    public delegate* unmanaged[Cdecl]<int> GetMouseDeltaX;
    public delegate* unmanaged[Cdecl]<int> GetMouseDeltaY;
    public delegate* unmanaged[Cdecl]<float> GetMouseWheelDelta;

    public delegate* unmanaged[Cdecl]<uint> GetWindowWidth;
    public delegate* unmanaged[Cdecl]<uint> GetWindowHeight;
    public delegate* unmanaged[Cdecl]<bool> HasFocus;

    public delegate* unmanaged[Cdecl]<uint> GetCharInputCount;
    public delegate* unmanaged[Cdecl]<uint, uint> GetCharInputAt;

    public delegate* unmanaged[Cdecl]<uint, uint, uint, Vector3*, bool> GetWorldTranslation;
    public delegate* unmanaged[Cdecl]<uint, uint, uint, Vector3*, bool> SetWorldTranslation;

    public delegate* unmanaged[Cdecl]<uint, uint, uint, Vector3*, bool> GetLocalTranslation;
    public delegate* unmanaged[Cdecl]<uint, uint, uint, Vector3*, bool> SetLocalTranslation;

    public delegate* unmanaged[Cdecl]<uint, uint, uint, Quaternion*, bool> GetWorldRotation;
    public delegate* unmanaged[Cdecl]<uint, uint, uint, Quaternion*, bool> SetWorldRotation;

    public delegate* unmanaged[Cdecl]<uint, uint, uint, Quaternion*, bool> GetLocalRotation;
    public delegate* unmanaged[Cdecl]<uint, uint, uint, Quaternion*, bool> SetLocalRotation;

    public delegate* unmanaged[Cdecl]<byte*, void> SetWindowTitle;
}

internal static unsafe class Native
{
    internal static NativeApi Api;

    internal static void SetApi(NativeApi api) => Api = api;

    internal static void LogInfo(string message) => CallLog(Api.LogInfo, message);

    internal static void LogError(string message) => CallLog(Api.LogError, message);

    internal static void SetWindowTitle(string title) => CallUtf8(Api.SetWindowTitle, title);

    private static void CallLog(delegate* unmanaged[Cdecl]<byte*, void> fn, string message)
    {
        CallUtf8(fn, message);
    }

    private static void CallUtf8(delegate* unmanaged[Cdecl]<byte*, void> fn, string message)
    {
        if (fn == null || string.IsNullOrEmpty(message))
        {
            return;
        }

        int byteCount = Encoding.UTF8.GetByteCount(message);
        Span<byte> buffer = byteCount + 1 <= 512
            ? stackalloc byte[byteCount + 1]
            : new byte[byteCount + 1];

        Encoding.UTF8.GetBytes(message, buffer);
        buffer[byteCount] = 0;

        fixed (byte* ptr = buffer)
        {
            fn(ptr);
        }
    }
}
