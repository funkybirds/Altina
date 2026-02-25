using System.Runtime.InteropServices;

namespace AltinaEngine.Managed;

[StructLayout(LayoutKind.Sequential)]
public struct Quaternion
{
    public float X;
    public float Y;
    public float Z;
    public float W;

    public Quaternion(float x, float y, float z, float w)
    {
        X = x;
        Y = y;
        Z = z;
        W = w;
    }

    public static Quaternion Identity => new(0.0f, 0.0f, 0.0f, 1.0f);
}

