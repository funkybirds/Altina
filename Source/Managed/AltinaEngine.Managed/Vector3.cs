using System.Runtime.InteropServices;

namespace AltinaEngine.Managed;

[StructLayout(LayoutKind.Sequential)]
public struct Vector3
{
    public float X;
    public float Y;
    public float Z;

    public Vector3(float x, float y, float z)
    {
        X = x;
        Y = y;
        Z = z;
    }

    public static Vector3 Zero => new(0.0f, 0.0f, 0.0f);
}
