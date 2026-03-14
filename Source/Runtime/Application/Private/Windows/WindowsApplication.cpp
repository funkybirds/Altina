#include "Base/AltinaBase.h"

#if AE_PLATFORM_WIN

    #include <Windows.h>
    #include <Windowsx.h>
    #include "Application/Windows/WindowsApplication.h"

    #include "Logging/Log.h"

namespace AltinaEngine::Application {
    namespace Container = Core::Container;
    using Container::FString;
    using Container::MakeUnique;
    using Container::MakeUniqueAs;

    namespace {
        constexpr const TChar* kWindowClassName = TEXT("AltinaEngineWindowClass");
        constexpr f32          kDefaultDpi      = 96.0f;
        using FGetDpiForWindowFn                = UINT(WINAPI*)(HWND);
        using FGetDpiForSystemFn                = UINT(WINAPI*)();
        using FAdjustWindowRectExForDpiFn       = BOOL(WINAPI*)(LPRECT, DWORD, BOOL, DWORD, UINT);
        using FSetProcessDpiAwarenessContextFn  = BOOL(WINAPI*)(DPI_AWARENESS_CONTEXT);
        using FSetProcessDpiAwareFn             = BOOL(WINAPI*)();

        void EnsureProcessDpiAwareness() {
            static bool sInitialized = false;
            if (sInitialized) {
                return;
            }
            sInitialized = true;

            const HMODULE user32 = GetModuleHandle(TEXT("user32.dll"));
            if (user32 == nullptr) {
                return;
            }

            auto* setContext = reinterpret_cast<FSetProcessDpiAwarenessContextFn>(
                GetProcAddress(user32, "SetProcessDpiAwarenessContext"));
            if (setContext != nullptr) {
                if (setContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2)) {
                    return;
                }
                if (setContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE)) {
                    return;
                }
                if (setContext(DPI_AWARENESS_CONTEXT_SYSTEM_AWARE)) {
                    return;
                }
            }

            auto* setAware = reinterpret_cast<FSetProcessDpiAwareFn>(
                GetProcAddress(user32, "SetProcessDPIAware"));
            if (setAware != nullptr) {
                setAware();
            }
        }

        [[nodiscard]] auto QueryWindowDpi(HWND windowHandle) -> u32 {
            const HMODULE user32 = GetModuleHandle(TEXT("user32.dll"));
            if (user32 == nullptr) {
                return static_cast<u32>(kDefaultDpi);
            }

            auto* getWindowDpi =
                reinterpret_cast<FGetDpiForWindowFn>(GetProcAddress(user32, "GetDpiForWindow"));
            if (getWindowDpi != nullptr && windowHandle != nullptr) {
                const UINT dpi = getWindowDpi(windowHandle);
                if (dpi > 0U) {
                    return static_cast<u32>(dpi);
                }
            }

            auto* getSystemDpi =
                reinterpret_cast<FGetDpiForSystemFn>(GetProcAddress(user32, "GetDpiForSystem"));
            if (getSystemDpi != nullptr) {
                const UINT dpi = getSystemDpi();
                if (dpi > 0U) {
                    return static_cast<u32>(dpi);
                }
            }
            return static_cast<u32>(kDefaultDpi);
        }

        [[nodiscard]] auto ClampMinClientSize(u32 value) -> u32 {
            return (value > 0U) ? value : 1U;
        }

        [[nodiscard]] auto ScaleLogicalToPhysical(u32 logicalSize, f32 dpiScale) -> u32 {
            const f32 safeScale = (dpiScale > 0.01f) ? dpiScale : 1.0f;
            const f32 scaled    = static_cast<f32>(ClampMinClientSize(logicalSize)) * safeScale;
            const u32 rounded   = static_cast<u32>(scaled + 0.5f);
            return (rounded > 0U) ? rounded : 1U;
        }

        void ResolveClientSizeForPolicy(const FPlatformWindowProperty& properties, f32 dpiScale,
            u32& outClientWidth, u32& outClientHeight) {
            if (properties.mDpiPolicy == EWindowDpiPolicy::LogicalFixed) {
                outClientWidth  = ScaleLogicalToPhysical(properties.mWidth, dpiScale);
                outClientHeight = ScaleLogicalToPhysical(properties.mHeight, dpiScale);
                return;
            }

            outClientWidth  = ClampMinClientSize(properties.mWidth);
            outClientHeight = ClampMinClientSize(properties.mHeight);
        }

        [[nodiscard]] auto ComputeWindowSizeFromClient(HWND windowHandle, DWORD style,
            u32 clientWidth, u32 clientHeight, u32 dpi, i32& outWidth, i32& outHeight) -> bool {
            RECT          windowRect{ 0, 0, static_cast<LONG>(ClampMinClientSize(clientWidth)),
                static_cast<LONG>(ClampMinClientSize(clientHeight)) };

            const DWORD   exStyle = static_cast<DWORD>(GetWindowLongPtr(windowHandle, GWL_EXSTYLE));
            const HMODULE user32  = GetModuleHandle(TEXT("user32.dll"));
            auto*         adjustForDpi = (user32 != nullptr)
                        ? reinterpret_cast<FAdjustWindowRectExForDpiFn>(
                      GetProcAddress(user32, "AdjustWindowRectExForDpi"))
                        : nullptr;
            BOOL          ok           = FALSE;
            if (adjustForDpi != nullptr && dpi > 0U) {
                ok = adjustForDpi(&windowRect, style, FALSE, exStyle, dpi);
            } else {
                ok = AdjustWindowRectEx(&windowRect, style, FALSE, exStyle);
            }
            if (!ok) {
                return false;
            }

            outWidth  = windowRect.right - windowRect.left;
            outHeight = windowRect.bottom - windowRect.top;
            return true;
        }

        auto ToWin32DisplayMode(EWindowDisplayMode DisplayMode) -> DWORD {
            switch (DisplayMode) {
                case EWindowDisplayMode::Fullscreen:
                    return WS_POPUP;
                case EWindowDisplayMode::Borderless:
                    return WS_POPUP | WS_CLIPCHILDREN | WS_CLIPSIBLINGS;
                case EWindowDisplayMode::Windowed:
                default:
                    return WS_OVERLAPPEDWINDOW;
            }
        }

        class FWindowTitleCStr {
        public:
            explicit FWindowTitleCStr(const FString& Title) : mBuffer(Title) {
                EnsureNullTerminated();
            }

            auto Get() const noexcept -> const TChar* {
                return mBuffer.IsEmptyString() ? TEXT("") : mBuffer.GetData();
            }

        private:
            void EnsureNullTerminated() {
                if (mBuffer.IsEmptyString()) {
                    mBuffer.Append(TEXT("AltinaEngine"));
                }

                const usize length = mBuffer.Length();
                if ((length == 0U) || (mBuffer[length - 1U] != static_cast<TChar>(0))) {
                    mBuffer.Append(static_cast<TChar>(0));
                }
            }

            FString mBuffer;
        };
    } // namespace

    FWindowsPlatformWindow::FWindowsPlatformWindow() {
        mInstanceHandle = static_cast<void*>(GetModuleHandle(nullptr));
        EnsureProcessDpiAwareness();
    }

    FWindowsPlatformWindow::~FWindowsPlatformWindow() {
        if (mWindowHandle) {
            DestroyWindow(static_cast<HWND>(mWindowHandle));
            mWindowHandle = nullptr;
        }
    }

    auto FWindowsPlatformWindow::Initialize(const FPlatformWindowProperty& InProperties) -> bool {
        RegisterWindowClass();

        const u32 initialDpi   = QueryWindowDpi(nullptr);
        const f32 initialScale = static_cast<f32>(initialDpi) / kDefaultDpi;
        u32       clientWidth  = ClampMinClientSize(InProperties.mWidth);
        u32       clientHeight = ClampMinClientSize(InProperties.mHeight);
        ResolveClientSizeForPolicy(InProperties, initialScale, clientWidth, clientHeight);

        const DWORD windowStyle = ResolveWindowStyle(InProperties);
        RECT windowRect{ 0, 0, static_cast<LONG>(clientWidth), static_cast<LONG>(clientHeight) };
        AdjustWindowRect(&windowRect, windowStyle, FALSE);

        const i32              width  = windowRect.right - windowRect.left;
        const i32              height = windowRect.bottom - windowRect.top;

        const FWindowTitleCStr titleCStr(InProperties.mTitle);

        mWindowHandle = static_cast<void*>(CreateWindowEx(0, kWindowClassName, titleCStr.Get(),
            windowStyle, CW_USEDEFAULT, CW_USEDEFAULT, width, height, nullptr, nullptr,
            static_cast<HINSTANCE>(mInstanceHandle), this));

        if (!mWindowHandle) {
            LogError(TEXT("Failed to create Win32 window (error {})."), GetLastError());
            return false;
        }

        SetWindowLongPtr(
            static_cast<HWND>(mWindowHandle), GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this));

        mProperties             = InProperties;
        const u32 dpi           = QueryWindowDpi(static_cast<HWND>(mWindowHandle));
        mProperties.mDpiScaling = static_cast<f32>(dpi) / kDefaultDpi;
        mProperties.mDpi        = dpi;
        UpdateCachedSizeFromClientRect();
        mIsClosed = false;
        if (mProperties.mDpiPolicy == EWindowDpiPolicy::LogicalFixed) {
            u32 expectedClientWidth  = 0U;
            u32 expectedClientHeight = 0U;
            ResolveClientSizeForPolicy(
                mProperties, mProperties.mDpiScaling, expectedClientWidth, expectedClientHeight);
            if (mCachedSize.mWidth != expectedClientWidth
                || mCachedSize.mHeight != expectedClientHeight) {
                Resize(mProperties.mWidth, mProperties.mHeight);
            }
        }

        LogInfo(TEXT("Window initialized DPI={} scale={} logical={}x{} physical={}x{} policy={}."),
            dpi, mProperties.mDpiScaling, mProperties.mWidth, mProperties.mHeight,
            mProperties.mPhysicalWidth, mProperties.mPhysicalHeight,
            static_cast<u32>(mProperties.mDpiPolicy));

        return true;
    }

    void FWindowsPlatformWindow::Show() {
        if (!mWindowHandle) {
            return;
        }

        ShowWindow(static_cast<HWND>(mWindowHandle), SW_SHOW);
        UpdateWindow(static_cast<HWND>(mWindowHandle));
    }

    void FWindowsPlatformWindow::Hide() {
        if (!mWindowHandle) {
            return;
        }

        ShowWindow(static_cast<HWND>(mWindowHandle), SW_HIDE);
    }

    void FWindowsPlatformWindow::Resize(u32 InWidth, u32 InHeight) {
        if (!mWindowHandle) {
            return;
        }

        mProperties.mWidth  = ClampMinClientSize(InWidth);
        mProperties.mHeight = ClampMinClientSize(InHeight);

        u32 targetClientWidth  = ClampMinClientSize(mProperties.mWidth);
        u32 targetClientHeight = ClampMinClientSize(mProperties.mHeight);
        ResolveClientSizeForPolicy(
            mProperties, mProperties.mDpiScaling, targetClientWidth, targetClientHeight);

        const auto  windowHandle = static_cast<HWND>(mWindowHandle);
        const DWORD windowStyle  = static_cast<DWORD>(GetWindowLongPtr(windowHandle, GWL_STYLE));
        i32         windowWidth  = static_cast<i32>(targetClientWidth);
        i32         windowHeight = static_cast<i32>(targetClientHeight);
        if (!ComputeWindowSizeFromClient(windowHandle, windowStyle, targetClientWidth,
                targetClientHeight, mProperties.mDpi, windowWidth, windowHeight)) {
            windowWidth  = static_cast<i32>(targetClientWidth);
            windowHeight = static_cast<i32>(targetClientHeight);
        }

        SetWindowPos(windowHandle, nullptr, 0, 0, windowWidth, windowHeight,
            SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
        UpdateCachedSizeFromClientRect();
    }

    void FWindowsPlatformWindow::MoveTo(i32 InPositionX, i32 InPositionY) {
        if (!mWindowHandle) {
            return;
        }

        SetWindowPos(static_cast<HWND>(mWindowHandle), nullptr, InPositionX, InPositionY, 0, 0,
            SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
    }

    void FWindowsPlatformWindow::Minimalize() {
        if (!mWindowHandle) {
            return;
        }

        ShowWindow(static_cast<HWND>(mWindowHandle), SW_MINIMIZE);
    }

    void FWindowsPlatformWindow::Maximalize() {
        if (!mWindowHandle) {
            return;
        }

        ShowWindow(static_cast<HWND>(mWindowHandle), SW_MAXIMIZE);
    }

    void FWindowsPlatformWindow::SetTitle(const FString& InTitle) {
        if (!mWindowHandle) {
            return;
        }

        const FWindowTitleCStr titleCStr(InTitle);
        SetWindowText(static_cast<HWND>(mWindowHandle), titleCStr.Get());

        mProperties.mTitle = InTitle;
    }

    void FWindowsPlatformWindow::SetCursorVisible(bool visible) {
        if (mCursorVisible == visible) {
            return;
        }
        mCursorVisible = visible;

        // ShowCursor maintains an internal display counter. Force it to the desired state.
        if (visible) {
            while (ShowCursor(TRUE) < 0) {}
        } else {
            while (ShowCursor(FALSE) >= 0) {}
        }
    }

    void FWindowsPlatformWindow::SetCursorClippedToClient(bool clipped) {
        if (!mWindowHandle) {
            return;
        }
        if (mCursorClipped == clipped) {
            return;
        }
        mCursorClipped = clipped;

        if (!clipped) {
            ClipCursor(nullptr);
            return;
        }

        RECT clientRect{};
        GetClientRect(static_cast<HWND>(mWindowHandle), &clientRect);

        // Convert to screen-space rectangle for ClipCursor.
        POINT tl{ clientRect.left, clientRect.top };
        POINT br{ clientRect.right, clientRect.bottom };
        ClientToScreen(static_cast<HWND>(mWindowHandle), &tl);
        ClientToScreen(static_cast<HWND>(mWindowHandle), &br);

        RECT screenRect{};
        screenRect.left   = tl.x;
        screenRect.top    = tl.y;
        screenRect.right  = br.x;
        screenRect.bottom = br.y;
        ClipCursor(&screenRect);
    }

    void FWindowsPlatformWindow::SetCursorPositionClient(i32 x, i32 y) {
        if (!mWindowHandle) {
            return;
        }

        POINT p{ x, y };
        ClientToScreen(static_cast<HWND>(mWindowHandle), &p);
        SetCursorPos(p.x, p.y);
    }

    auto FWindowsPlatformWindow::GetSize() const noexcept -> FWindowExtent { return mCachedSize; }

    auto FWindowsPlatformWindow::GetProperties() const -> FPlatformWindowProperty {
        if (mWindowHandle) {
            const_cast<FWindowsPlatformWindow*>(this)->UpdateCachedSizeFromClientRect();
        }
        return mProperties;
    }

    auto FWindowsPlatformWindow::GetNativeHandle() const noexcept -> void* { return mWindowHandle; }

    auto FWindowsPlatformWindow::GetWindowHandle() const noexcept -> void* { return mWindowHandle; }

    void FWindowsPlatformWindow::SetMessageRouter(FAppMessageRouter* InRouter) noexcept {
        mMessageRouter = InRouter;
    }

    LRESULT CALLBACK FWindowsPlatformWindow::WindowProc(
        HWND InWindowHandle, UINT InMessage, WPARAM InWParam, LPARAM InLParam) {
        FWindowsPlatformWindow* window = nullptr;

        if (InMessage == WM_NCCREATE) {
            const auto* createStruct = reinterpret_cast<CREATESTRUCT*>(InLParam);
            window = static_cast<FWindowsPlatformWindow*>(createStruct->lpCreateParams);
            SetWindowLongPtr(InWindowHandle, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(window));
        } else {
            window = reinterpret_cast<FWindowsPlatformWindow*>(
                GetWindowLongPtr(InWindowHandle, GWLP_USERDATA));
        }

        if (window) {
            window->mWindowHandle = static_cast<void*>(InWindowHandle);
        }

        const auto dispatchIfAvailable = [window](auto&& dispatchFn) {
            if (window != nullptr && window->mMessageRouter != nullptr) {
                dispatchFn(*window->mMessageRouter);
            }
        };

        switch (InMessage) {
            case WM_CLOSE:
                if (window) {
                    window->mIsClosed = true;
                }
                dispatchIfAvailable([window](FAppMessageRouter& router) {
                    router.BroadcastWindowCloseRequested(window);
                });
                break;
            case WM_DESTROY:
                if (window) {
                    window->mIsClosed = true;
                }
                dispatchIfAvailable(
                    [window](FAppMessageRouter& router) { router.BroadcastWindowClosed(window); });
                PostQuitMessage(0);
                return 0;
            case WM_SIZE:
                if (window) {
                    window->UpdateCachedSizeFromClientRect();
                    if (InWParam != SIZE_MINIMIZED
                        && window->mProperties.mDpiPolicy == EWindowDpiPolicy::LogicalFixed
                        && window->mProperties.mDpiScaling > 0.01f
                        && window->mCachedSize.mWidth > 0U && window->mCachedSize.mHeight > 0U) {
                        window->mProperties.mWidth = ScaleLogicalToPhysical(
                            window->mCachedSize.mWidth, 1.0f / window->mProperties.mDpiScaling);
                        window->mProperties.mHeight = ScaleLogicalToPhysical(
                            window->mCachedSize.mHeight, 1.0f / window->mProperties.mDpiScaling);
                    }
                    const auto extent = window->GetSize();
                    dispatchIfAvailable([window, &extent](FAppMessageRouter& router) {
                        router.BroadcastWindowResized(window, extent);
                    });

                    dispatchIfAvailable([window, InWParam](FAppMessageRouter& router) {
                        switch (InWParam) {
                            case SIZE_MINIMIZED:
                                router.BroadcastWindowMinimized(window);
                                break;
                            case SIZE_MAXIMIZED:
                                router.BroadcastWindowMaximized(window);
                                break;
                            case SIZE_RESTORED:
                                router.BroadcastWindowRestored(window);
                                break;
                            default:
                                break;
                        }
                    });
                }
                break;
            case WM_MOVE:
            {
                const i32 positionX = static_cast<i32>(GET_X_LPARAM(InLParam));
                const i32 positionY = static_cast<i32>(GET_Y_LPARAM(InLParam));
                dispatchIfAvailable([window, positionX, positionY](FAppMessageRouter& router) {
                    router.BroadcastWindowMoved(window, positionX, positionY);
                });
                break;
            }
            case WM_SETFOCUS:
                dispatchIfAvailable([window](FAppMessageRouter& router) {
                    router.BroadcastWindowFocusGained(window);
                });
                break;
            case WM_KILLFOCUS:
                dispatchIfAvailable([window](FAppMessageRouter& router) {
                    router.BroadcastWindowFocusLost(window);
                });
                break;
            case WM_DPICHANGED:
                if (window) {
                    const u32 dpiX                  = LOWORD(InWParam);
                    const f32 dpiScale              = static_cast<f32>(dpiX) / kDefaultDpi;
                    window->mProperties.mDpiScaling = dpiScale;
                    window->mProperties.mDpi        = dpiX;
                    dispatchIfAvailable([window, dpiScale](FAppMessageRouter& router) {
                        router.BroadcastWindowDpiScaleChanged(window, dpiScale);
                    });

                    const RECT* suggestedRect = reinterpret_cast<const RECT*>(InLParam);
                    if (window->mProperties.mDpiPolicy == EWindowDpiPolicy::LogicalFixed) {
                        u32 targetClientWidth  = 0U;
                        u32 targetClientHeight = 0U;
                        ResolveClientSizeForPolicy(
                            window->mProperties, dpiScale, targetClientWidth, targetClientHeight);
                        const DWORD windowStyle =
                            static_cast<DWORD>(GetWindowLongPtr(InWindowHandle, GWL_STYLE));
                        i32 windowWidth  = static_cast<i32>(targetClientWidth);
                        i32 windowHeight = static_cast<i32>(targetClientHeight);
                        if (!ComputeWindowSizeFromClient(InWindowHandle, windowStyle,
                                targetClientWidth, targetClientHeight, dpiX, windowWidth,
                                windowHeight)) {
                            windowWidth  = static_cast<i32>(targetClientWidth);
                            windowHeight = static_cast<i32>(targetClientHeight);
                        }

                        RECT currentRect{};
                        GetWindowRect(InWindowHandle, &currentRect);
                        i32 posX = currentRect.left;
                        i32 posY = currentRect.top;
                        if (suggestedRect != nullptr) {
                            posX = suggestedRect->left;
                            posY = suggestedRect->top;
                        }
                        SetWindowPos(InWindowHandle, nullptr, posX, posY, windowWidth, windowHeight,
                            SWP_NOZORDER | SWP_NOACTIVATE);
                    } else if (suggestedRect != nullptr) {
                        SetWindowPos(InWindowHandle, nullptr, suggestedRect->left,
                            suggestedRect->top, suggestedRect->right - suggestedRect->left,
                            suggestedRect->bottom - suggestedRect->top,
                            SWP_NOZORDER | SWP_NOACTIVATE);
                    }
                    window->UpdateCachedSizeFromClientRect();
                    LogInfo(
                        TEXT(
                            "Window DPI changed to {} (scale={}) logical={}x{} physical={}x{} policy={}."),
                        dpiX, dpiScale, window->mProperties.mWidth, window->mProperties.mHeight,
                        window->mProperties.mPhysicalWidth, window->mProperties.mPhysicalHeight,
                        static_cast<u32>(window->mProperties.mDpiPolicy));
                }
                break;
            case WM_KEYDOWN:
            {
                const bool isRepeat = (InLParam & (1LL << 30)) != 0;
                const u32  keyCode  = static_cast<u32>(InWParam);
                dispatchIfAvailable([keyCode, isRepeat](FAppMessageRouter& router) {
                    router.BroadcastKeyDown(keyCode, isRepeat);
                });
                break;
            }
            case WM_KEYUP:
            {
                const u32 keyCode = static_cast<u32>(InWParam);
                dispatchIfAvailable(
                    [keyCode](FAppMessageRouter& router) { router.BroadcastKeyUp(keyCode); });
                break;
            }
            case WM_SYSKEYDOWN:
            {
                const bool isRepeat = (InLParam & (1LL << 30)) != 0;
                const u32  keyCode  = static_cast<u32>(InWParam);
                // System key messages may include menu accelerators (for example VK_SPACE/F10)
                // that are not intended as gameplay/script input. Only forward Alt keys.
                if (keyCode == VK_MENU || keyCode == VK_LMENU || keyCode == VK_RMENU) {
                    dispatchIfAvailable([keyCode, isRepeat](FAppMessageRouter& router) {
                        router.BroadcastKeyDown(keyCode, isRepeat);
                    });
                }
                break;
            }
            case WM_SYSKEYUP:
            {
                const u32 keyCode = static_cast<u32>(InWParam);
                if (keyCode == VK_MENU || keyCode == VK_LMENU || keyCode == VK_RMENU) {
                    dispatchIfAvailable(
                        [keyCode](FAppMessageRouter& router) { router.BroadcastKeyUp(keyCode); });
                }
                break;
            }
            case WM_CHAR:
            {
                const u32 charCode = static_cast<u32>(InWParam);
                dispatchIfAvailable(
                    [charCode](FAppMessageRouter& router) { router.BroadcastCharInput(charCode); });
                break;
            }
            case WM_MOUSEMOVE:
            {
                const i32 positionX = static_cast<i32>(GET_X_LPARAM(InLParam));
                const i32 positionY = static_cast<i32>(GET_Y_LPARAM(InLParam));
                if (window && !window->mIsMouseTracking) {
                    TRACKMOUSEEVENT trackEvent{};
                    trackEvent.cbSize    = sizeof(TRACKMOUSEEVENT);
                    trackEvent.dwFlags   = TME_LEAVE;
                    trackEvent.hwndTrack = InWindowHandle;
                    if (TrackMouseEvent(&trackEvent)) {
                        window->mIsMouseTracking = true;
                        dispatchIfAvailable(
                            [](FAppMessageRouter& router) { router.BroadcastMouseEnter(); });
                    }
                }
                dispatchIfAvailable([positionX, positionY](FAppMessageRouter& router) {
                    router.BroadcastMouseMove(positionX, positionY);
                });
                break;
            }
            case WM_MOUSELEAVE:
                if (window) {
                    window->mIsMouseTracking = false;
                }
                dispatchIfAvailable(
                    [](FAppMessageRouter& router) { router.BroadcastMouseLeave(); });
                break;
            case WM_LBUTTONDOWN:
                dispatchIfAvailable(
                    [](FAppMessageRouter& router) { router.BroadcastMouseButtonDown(0U); });
                break;
            case WM_LBUTTONUP:
                dispatchIfAvailable(
                    [](FAppMessageRouter& router) { router.BroadcastMouseButtonUp(0U); });
                break;
            case WM_RBUTTONDOWN:
                dispatchIfAvailable(
                    [](FAppMessageRouter& router) { router.BroadcastMouseButtonDown(1U); });
                break;
            case WM_RBUTTONUP:
                dispatchIfAvailable(
                    [](FAppMessageRouter& router) { router.BroadcastMouseButtonUp(1U); });
                break;
            case WM_MBUTTONDOWN:
                dispatchIfAvailable(
                    [](FAppMessageRouter& router) { router.BroadcastMouseButtonDown(2U); });
                break;
            case WM_MBUTTONUP:
                dispatchIfAvailable(
                    [](FAppMessageRouter& router) { router.BroadcastMouseButtonUp(2U); });
                break;
            case WM_XBUTTONDOWN:
            {
                const u32 button = (GET_XBUTTON_WPARAM(InWParam) == XBUTTON2) ? 4U : 3U;
                dispatchIfAvailable([button](FAppMessageRouter& router) {
                    router.BroadcastMouseButtonDown(button);
                });
                break;
            }
            case WM_XBUTTONUP:
            {
                const u32 button = (GET_XBUTTON_WPARAM(InWParam) == XBUTTON2) ? 4U : 3U;
                dispatchIfAvailable(
                    [button](FAppMessageRouter& router) { router.BroadcastMouseButtonUp(button); });
                break;
            }
            case WM_MOUSEWHEEL:
            {
                const f32 delta = static_cast<f32>(GET_WHEEL_DELTA_WPARAM(InWParam))
                    / static_cast<f32>(WHEEL_DELTA);
                dispatchIfAvailable(
                    [delta](FAppMessageRouter& router) { router.BroadcastMouseWheel(delta); });
                break;
            }
            default:
                break;
        }

        return DefWindowProc(InWindowHandle, InMessage, InWParam, InLParam);
    }

    void FWindowsPlatformWindow::RegisterWindowClass() {
        static bool bClassRegistered = false;
        if (bClassRegistered) {
            return;
        }

        WNDCLASS windowClass{};
        windowClass.style         = CS_HREDRAW | CS_VREDRAW;
        windowClass.lpfnWndProc   = &FWindowsPlatformWindow::WindowProc;
        windowClass.hInstance     = static_cast<HINSTANCE>(mInstanceHandle);
        windowClass.lpszClassName = kWindowClassName;
        windowClass.hCursor       = LoadCursor(nullptr, IDC_ARROW);
        windowClass.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);

        if (!RegisterClass(&windowClass)) {
            const DWORD errorCode = GetLastError();
            if (errorCode != ERROR_CLASS_ALREADY_EXISTS) {
                LogError(TEXT("RegisterClass failed (error {})."), errorCode);
            }
        }

        bClassRegistered = true;
    }

    void FWindowsPlatformWindow::UpdateCachedSizeFromClientRect() {
        if (!mWindowHandle) {
            return;
        }

        RECT clientRect{};
        GetClientRect(static_cast<HWND>(mWindowHandle), &clientRect);
        mCachedSize.mWidth          = static_cast<u32>(clientRect.right - clientRect.left);
        mCachedSize.mHeight         = static_cast<u32>(clientRect.bottom - clientRect.top);
        mProperties.mPhysicalWidth  = mCachedSize.mWidth;
        mProperties.mPhysicalHeight = mCachedSize.mHeight;
    }

    DWORD FWindowsPlatformWindow::ResolveWindowStyle(
        const FPlatformWindowProperty& InProperties) const noexcept {
        return ToWin32DisplayMode(InProperties.mDisplayMode);
    }

    FWindowsApplication::FWindowsApplication(const FStartupParameters& InStartupParameters)
        : FApplication(InStartupParameters) {}

    auto FWindowsApplication::CreatePlatformWindow() -> FWindowOwner {
        auto platformWindow = MakeUniqueAs<FPlatformWindow, FWindowsPlatformWindow>();
        if (auto* nativeWindow = static_cast<FWindowsPlatformWindow*>(platformWindow.Get())) {
            nativeWindow->SetMessageRouter(GetMessageRouter());
        }
        return platformWindow;
    }

    void FWindowsApplication::PumpPlatformMessages() {
        MSG message{};
        while (PeekMessage(&message, nullptr, 0, 0, PM_REMOVE)) {
            if (message.message == WM_QUIT) {
                RequestShutdown();
                break;
            }

            TranslateMessage(&message);
            DispatchMessage(&message);
        }
    }

} // namespace AltinaEngine::Application

#endif // AE_PLATFORM_WIN
