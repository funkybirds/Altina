namespace AltinaEngine.Managed;

public static class ManagedLog
{
    public static void Info(string message) => Native.LogInfo(message);

    public static void Error(string message) => Native.LogError(message);
}
