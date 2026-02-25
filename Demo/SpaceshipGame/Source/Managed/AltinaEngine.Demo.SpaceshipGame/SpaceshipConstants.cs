namespace AltinaEngine.Demo.SpaceshipGame;

internal static class SpaceshipConstants
{
    // Scale: 1 unit = 10,000 km (approx). Some values are intentionally compressed for readability.
    public const float EarthRadius = 0.6371f;
    public const float MoonRadius = 0.1737f;
    public const float SunRadius = 6.9634f;

    public const float EarthMoonDistance = 38.44f;
    public const float SunEarthDistance = 400.0f;

    // Ship/orbit tuning (gameplay constants).
    public const float ShipRadius = 0.05f;
    public const float EarthOrbitRadius = 1.2f;
    public const float MoonOrbitRadius = 0.55f;

    // Revolution speeds (radians/sec). Not physical; tuned for demo readability.
    public const float EarthRevolutionSpeed = 0.03f;
    public const float MoonRevolutionSpeed = 0.25f;

    // Transfer orbit window/gating.
    public const float TransferWindowHalfAngleRad = 15.0f * (3.14159265f / 180.0f);
    public const float TransferMoonCaptureDistance = 1.6f;
}

