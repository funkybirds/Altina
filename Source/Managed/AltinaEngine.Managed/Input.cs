namespace AltinaEngine.Managed;

public enum EKey : ushort
{
    Unknown = 0,

    A,
    B,
    C,
    D,
    E,
    F,
    G,
    H,
    I,
    J,
    K,
    L,
    M,
    N,
    O,
    P,
    Q,
    R,
    S,
    T,
    U,
    V,
    W,
    X,
    Y,
    Z,

    Num0,
    Num1,
    Num2,
    Num3,
    Num4,
    Num5,
    Num6,
    Num7,
    Num8,
    Num9,

    Escape,
    Space,
    Enter,
    Tab,
    Backspace,

    Left,
    Right,
    Up,
    Down,

    LeftShift,
    RightShift,
    LeftControl,
    RightControl,
    LeftAlt,
    RightAlt,
}

public static unsafe class Input
{
    public static bool IsKeyDown(EKey key)
    {
        var fn = Native.Api.IsKeyDown;
        return fn != null && fn((ushort)key);
    }

    public static bool WasKeyPressed(EKey key)
    {
        var fn = Native.Api.WasKeyPressed;
        return fn != null && fn((ushort)key);
    }

    public static bool WasKeyReleased(EKey key)
    {
        var fn = Native.Api.WasKeyReleased;
        return fn != null && fn((ushort)key);
    }

    public static bool IsMouseButtonDown(uint button)
    {
        var fn = Native.Api.IsMouseButtonDown;
        return fn != null && fn(button);
    }

    public static bool WasMouseButtonPressed(uint button)
    {
        var fn = Native.Api.WasMouseButtonPressed;
        return fn != null && fn(button);
    }

    public static bool WasMouseButtonReleased(uint button)
    {
        var fn = Native.Api.WasMouseButtonReleased;
        return fn != null && fn(button);
    }

    public static int MouseX => Native.Api.GetMouseX != null ? Native.Api.GetMouseX() : 0;
    public static int MouseY => Native.Api.GetMouseY != null ? Native.Api.GetMouseY() : 0;
    public static int MouseDeltaX => Native.Api.GetMouseDeltaX != null ? Native.Api.GetMouseDeltaX() : 0;
    public static int MouseDeltaY => Native.Api.GetMouseDeltaY != null ? Native.Api.GetMouseDeltaY() : 0;
    public static float MouseWheelDelta => Native.Api.GetMouseWheelDelta != null ? Native.Api.GetMouseWheelDelta() : 0.0f;

    public static uint WindowWidth => Native.Api.GetWindowWidth != null ? Native.Api.GetWindowWidth() : 0;
    public static uint WindowHeight => Native.Api.GetWindowHeight != null ? Native.Api.GetWindowHeight() : 0;
    public static bool HasFocus => Native.Api.HasFocus != null && Native.Api.HasFocus();

    public static uint CharInputCount => Native.Api.GetCharInputCount != null ? Native.Api.GetCharInputCount() : 0;

    public static uint GetCharInputAt(uint index)
    {
        var fn = Native.Api.GetCharInputAt;
        return fn != null ? fn(index) : 0;
    }
}
