#include "Base/AltinaBase.h"

#if AE_PLATFORM_WIN

    #include <Windows.h>
    #include "Application/Windows/WindowsApplication.h"

    #include "Logging/Log.h"

namespace AltinaEngine::Application
{
    using AltinaEngine::Core::Container::FString;
    using AltinaEngine::Core::Container::MakeUnique;

    namespace
    {
        constexpr const TChar* kWindowClassName = TEXT("AltinaEngineWindowClass");

        DWORD                  ToWin32DisplayMode(EWindowDisplayMode DisplayMode)
        {
            switch (DisplayMode)
            {
                case EWindowDisplayMode::Fullscreen:
                    return WS_POPUP;
                case EWindowDisplayMode::Borderless:
                    return WS_POPUP | WS_CLIPCHILDREN | WS_CLIPSIBLINGS;
                case EWindowDisplayMode::Windowed:
                default:
                    return WS_OVERLAPPEDWINDOW;
            }
        }

        class FWindowTitleCStr
        {
        public:
            explicit FWindowTitleCStr(const FString& Title) : mBuffer(Title) { EnsureNullTerminated(); }

            const TChar* Get() const noexcept { return mBuffer.IsEmptyString() ? TEXT("") : mBuffer.GetData(); }

        private:
            void EnsureNullTerminated()
            {
                if (mBuffer.IsEmptyString())
                {
                    mBuffer.Append(TEXT("AltinaEngine"));
                }

                const AltinaEngine::usize Length = mBuffer.Length();
                if ((Length == 0U) || (mBuffer[Length - 1U] != static_cast<TChar>(0)))
                {
                    mBuffer.Append(static_cast<TChar>(0));
                }
            }

            FString mBuffer;
        };
    } // namespace

    FWindowsPlatformWindow::FWindowsPlatformWindow() { mInstanceHandle = static_cast<void*>(GetModuleHandle(nullptr)); }

    FWindowsPlatformWindow::~FWindowsPlatformWindow()
    {
        if (mWindowHandle)
        {
            DestroyWindow(static_cast<HWND>(mWindowHandle));
            mWindowHandle = nullptr;
        }
    }

    bool FWindowsPlatformWindow::Initialize(const FPlatformWindowProperty& InProperties)
    {
        RegisterWindowClass();

        const DWORD WindowStyle = ResolveWindowStyle(InProperties);
        RECT        WindowRect{ 0, 0, static_cast<LONG>(InProperties.Width), static_cast<LONG>(InProperties.Height) };
        AdjustWindowRect(&WindowRect, WindowStyle, FALSE);

        const i32              Width  = WindowRect.right - WindowRect.left;
        const i32              Height = WindowRect.bottom - WindowRect.top;

        const FWindowTitleCStr TitleCStr(InProperties.Title);

        mWindowHandle =
            static_cast<void*>(CreateWindowEx(0, kWindowClassName, TitleCStr.Get(), WindowStyle, CW_USEDEFAULT,
                CW_USEDEFAULT, Width, Height, nullptr, nullptr, static_cast<HINSTANCE>(mInstanceHandle), this));

        if (!mWindowHandle)
        {
            LogError(TEXT("Failed to create Win32 window (error {})."), GetLastError());
            return false;
        }

        SetWindowLongPtr(static_cast<HWND>(mWindowHandle), GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this));

        mProperties = InProperties;
        UpdateCachedSizeFromClientRect();

        return true;
    }

    void FWindowsPlatformWindow::Show()
    {
        if (!mWindowHandle)
        {
            return;
        }

        ShowWindow(static_cast<HWND>(mWindowHandle), SW_SHOW);
        UpdateWindow(static_cast<HWND>(mWindowHandle));
    }

    void FWindowsPlatformWindow::Hide()
    {
        if (!mWindowHandle)
        {
            return;
        }

        ShowWindow(static_cast<HWND>(mWindowHandle), SW_HIDE);
    }

    void FWindowsPlatformWindow::Resize(u32 InWidth, u32 InHeight)
    {
        if (!mWindowHandle)
        {
            return;
        }

        mProperties.Width  = InWidth;
        mProperties.Height = InHeight;

        SetWindowPos(static_cast<HWND>(mWindowHandle), nullptr, 0, 0, static_cast<int>(InWidth),
            static_cast<int>(InHeight), SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
        UpdateCachedSizeFromClientRect();
    }

    void FWindowsPlatformWindow::MoveTo(i32 InPositionX, i32 InPositionY)
    {
        if (!mWindowHandle)
        {
            return;
        }

        SetWindowPos(static_cast<HWND>(mWindowHandle), nullptr, InPositionX, InPositionY, 0, 0,
            SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
    }

    void FWindowsPlatformWindow::Minimalize()
    {
        if (!mWindowHandle)
        {
            return;
        }

        ShowWindow(static_cast<HWND>(mWindowHandle), SW_MINIMIZE);
    }

    void FWindowsPlatformWindow::Maximalize()
    {
        if (!mWindowHandle)
        {
            return;
        }

        ShowWindow(static_cast<HWND>(mWindowHandle), SW_MAXIMIZE);
    }

    FWindowExtent           FWindowsPlatformWindow::GetSize() const noexcept { return mCachedSize; }

    FPlatformWindowProperty FWindowsPlatformWindow::GetProperties() const
    {
        if (mWindowHandle)
        {
            const_cast<FWindowsPlatformWindow*>(this)->UpdateCachedSizeFromClientRect();
        }
        return mProperties;
    }

    void*            FWindowsPlatformWindow::GetWindowHandle() const noexcept { return mWindowHandle; }

    LRESULT CALLBACK FWindowsPlatformWindow::WindowProc(
        HWND InWindowHandle, UINT InMessage, WPARAM InWParam, LPARAM InLParam)
    {
        FWindowsPlatformWindow* Window = nullptr;

        if (InMessage == WM_NCCREATE)
        {
            const auto* CreateStruct = reinterpret_cast<CREATESTRUCT*>(InLParam);
            Window                   = static_cast<FWindowsPlatformWindow*>(CreateStruct->lpCreateParams);
            SetWindowLongPtr(InWindowHandle, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(Window));
        }
        else
        {
            Window = reinterpret_cast<FWindowsPlatformWindow*>(GetWindowLongPtr(InWindowHandle, GWLP_USERDATA));
        }

        if (Window)
        {
            Window->mWindowHandle = static_cast<void*>(InWindowHandle);
        }

        switch (InMessage)
        {
            case WM_DESTROY:
                PostQuitMessage(0);
                return 0;
            case WM_SIZE:
                if (Window)
                {
                    Window->UpdateCachedSizeFromClientRect();
                }
                break;
            default:
                break;
        }

        return DefWindowProc(InWindowHandle, InMessage, InWParam, InLParam);
    }

    void FWindowsPlatformWindow::RegisterWindowClass()
    {
        static bool bClassRegistered = false;
        if (bClassRegistered)
        {
            return;
        }

        WNDCLASS WindowClass{};
        WindowClass.style         = CS_HREDRAW | CS_VREDRAW;
        WindowClass.lpfnWndProc   = &FWindowsPlatformWindow::WindowProc;
        WindowClass.hInstance     = static_cast<HINSTANCE>(mInstanceHandle);
        WindowClass.lpszClassName = kWindowClassName;
        WindowClass.hCursor       = LoadCursor(nullptr, IDC_ARROW);
        WindowClass.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);

        if (!RegisterClass(&WindowClass))
        {
            const DWORD ErrorCode = GetLastError();
            if (ErrorCode != ERROR_CLASS_ALREADY_EXISTS)
            {
                LogError(TEXT("RegisterClass failed (error {})."), ErrorCode);
            }
        }

        bClassRegistered = true;
    }

    void FWindowsPlatformWindow::UpdateCachedSizeFromClientRect()
    {
        if (!mWindowHandle)
        {
            return;
        }

        RECT ClientRect{};
        GetClientRect(static_cast<HWND>(mWindowHandle), &ClientRect);
        mCachedSize.Width  = static_cast<u32>(ClientRect.right - ClientRect.left);
        mCachedSize.Height = static_cast<u32>(ClientRect.bottom - ClientRect.top);
        mProperties.Width  = mCachedSize.Width;
        mProperties.Height = mCachedSize.Height;
    }

    DWORD FWindowsPlatformWindow::ResolveWindowStyle(const FPlatformWindowProperty& InProperties) const noexcept
    {
        return ToWin32DisplayMode(InProperties.DisplayMode);
    }

    FWindowsApplication::FWindowsApplication(const FStartupParameters& InStartupParameters)
        : FApplication(InStartupParameters)
    {
    }

    FWindowOwner FWindowsApplication::CreatePlatformWindow()
    {
        auto Window = MakeUnique<FWindowsPlatformWindow>();
        return FWindowOwner(Window.release());
    }

    void FWindowsApplication::PumpPlatformMessages()
    {
        MSG Message{};
        while (PeekMessage(&Message, nullptr, 0, 0, PM_REMOVE))
        {
            if (Message.message == WM_QUIT)
            {
                RequestShutdown();
                break;
            }

            TranslateMessage(&Message);
            DispatchMessage(&Message);
        }
    }

} // namespace AltinaEngine::Application

#endif // AE_PLATFORM_WIN
