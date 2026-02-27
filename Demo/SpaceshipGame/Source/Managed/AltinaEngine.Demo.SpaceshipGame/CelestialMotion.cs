using AltinaEngine.Managed;

namespace AltinaEngine.Demo.SpaceshipGame;

internal static class CelestialMotion
{
    public static Vector3 SunPosition(float t)
    {
        _ = t;
        return Vector3.Zero;
    }

    public static Vector3 EarthPosition(float t)
    {
        // Static bodies for v1 prototype: keep Earth/Moon fixed.
        _ = t;
        return new Vector3(SpaceshipConstants.SunEarthDistance, 0.0f, 0.0f);
    }

    public static Vector3 MoonPosition(float t)
    {
        _ = t;
        Vector3 earth = EarthPosition(0.0f);
        return SpaceshipMath.Add(earth, new Vector3(SpaceshipConstants.EarthMoonDistance, 0.0f, 0.0f));
    }
}
