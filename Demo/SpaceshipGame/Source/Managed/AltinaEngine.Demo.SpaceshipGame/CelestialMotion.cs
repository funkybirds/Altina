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
        // Earth revolves around the Sun on the XZ plane.
        Vector3 offset = new(SpaceshipConstants.SunEarthDistance, 0.0f, 0.0f);
        return SpaceshipMath.RotateY(offset, t * SpaceshipConstants.EarthRevolutionSpeed);
    }

    public static Vector3 MoonPosition(float t)
    {
        // Moon revolves around the Earth on the XZ plane.
        Vector3 earth = EarthPosition(t);
        Vector3 offset = new(SpaceshipConstants.EarthMoonDistance, 0.0f, 0.0f);
        Vector3 local = SpaceshipMath.RotateY(offset, t * SpaceshipConstants.MoonRevolutionSpeed);
        return SpaceshipMath.Add(earth, local);
    }
}
