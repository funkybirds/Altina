namespace AltinaEngine.Managed;

public static class Window
{
    public static void SetTitle(string title) => Native.SetWindowTitle(title);
}

