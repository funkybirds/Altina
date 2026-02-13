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

        auto                   ToWin32DisplayMode(EWindowDisplayMode DisplayMode) -> DWORD {
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
    }

    FWindowsPlatformWindow::~FWindowsPlatformWindow() {
        if (mWindowHandle) {
            DestroyWindow(static_cast<HWND>(mWindowHandle));
            mWindowHandle = nullptr;
        }
    }

    auto FWindowsPlatformWindow::Initialize(const FPlatformWindowProperty& InProperties) -> bool {
        RegisterWindowClass();

        const DWORD windowStyle = ResolveWindowStyle(InProperties);
        RECT        windowRect{ 0, 0, static_cast<LONG>(InProperties.mWidth),
            static_cast<LONG>(InProperties.mHeight) };
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

        mProperties = InProperties;
        UpdateCachedSizeFromClientRect();

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

        mProperties.mWidth  = InWidth;
        mProperties.mHeight = InHeight;

        SetWindowPos(static_cast<HWND>(mWindowHandle), nullptr, 0, 0, static_cast<int>(InWidth),
            static_cast<int>(InHeight), SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
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
                dispatchIfAvailable([window](FAppMessageRouter& router) {
                    router.BroadcastWindowCloseRequested(window);
                });
                break;
            case WM_DESTROY:
                dispatchIfAvailable(
                    [window](FAppMessageRouter& router) { router.BroadcastWindowClosed(window); });
                PostQuitMessage(0);
                return 0;
            case WM_SIZE:
                if (window) {
                    window->UpdateCachedSizeFromClientRect();
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
                    dispatchIfAvailable([window, dpiScale](FAppMessageRouter& router) {
                        router.BroadcastWindowDpiScaleChanged(window, dpiScale);
                    });

                    const RECT* suggestedRect = reinterpret_cast<const RECT*>(InLParam);
                    if (suggestedRect != nullptr) {
                        SetWindowPos(InWindowHandle, nullptr, suggestedRect->left,
                            suggestedRect->top, suggestedRect->right - suggestedRect->left,
                            suggestedRect->bottom - suggestedRect->top,
                            SWP_NOZORDER | SWP_NOACTIVATE);
                    }
                }
                break;
            case WM_KEYDOWN:
            case WM_SYSKEYDOWN:
            {
                const bool isRepeat = (InLParam & (1LL << 30)) != 0;
                const u32  keyCode  = static_cast<u32>(InWParam);
                dispatchIfAvailable([keyCode, isRepeat](FAppMessageRouter& router) {
                    router.BroadcastKeyDown(keyCode, isRepeat);
                });
                break;
            }
            case WM_KEYUP:
            case WM_SYSKEYUP:
            {
                const u32 keyCode = static_cast<u32>(InWParam);
                dispatchIfAvailable(
                    [keyCode](FAppMessageRouter& router) { router.BroadcastKeyUp(keyCode); });
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
        mCachedSize.mWidth  = static_cast<u32>(clientRect.right - clientRect.left);
        mCachedSize.mHeight = static_cast<u32>(clientRect.bottom - clientRect.top);
        mProperties.mWidth  = mCachedSize.mWidth;
        mProperties.mHeight = mCachedSize.mHeight;
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
