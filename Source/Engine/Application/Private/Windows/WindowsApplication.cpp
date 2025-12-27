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

        auto                   ToWin32DisplayMode(EWindowDisplayMode DisplayMode) -> DWORD
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

            auto Get() const noexcept -> const TChar* { return mBuffer.IsEmptyString() ? TEXT("") : mBuffer.GetData(); }

        private:
            void EnsureNullTerminated()
            {
                if (mBuffer.IsEmptyString())
                {
                    mBuffer.Append(TEXT("AltinaEngine"));
                }

                const usize length = mBuffer.Length();
                if ((length == 0U) || (mBuffer[length - 1U] != static_cast<TChar>(0)))
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

    auto FWindowsPlatformWindow::Initialize(const FPlatformWindowProperty& InProperties) -> bool
    {
        RegisterWindowClass();

        const DWORD windowStyle = ResolveWindowStyle(InProperties);
        RECT        windowRect{ 0, 0, static_cast<LONG>(InProperties.mWidth), static_cast<LONG>(InProperties.mHeight) };
        AdjustWindowRect(&windowRect, windowStyle, FALSE);

        const i32              width  = windowRect.right - windowRect.left;
        const i32              height = windowRect.bottom - windowRect.top;

        const FWindowTitleCStr titleCStr(InProperties.mTitle);

        mWindowHandle =
            static_cast<void*>(CreateWindowEx(0, kWindowClassName, titleCStr.Get(), windowStyle, CW_USEDEFAULT,
                CW_USEDEFAULT, width, height, nullptr, nullptr, static_cast<HINSTANCE>(mInstanceHandle), this));

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

        mProperties.mWidth  = InWidth;
        mProperties.mHeight = InHeight;

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

    auto FWindowsPlatformWindow::GetSize() const noexcept -> FWindowExtent { return mCachedSize; }

    auto FWindowsPlatformWindow::GetProperties() const -> FPlatformWindowProperty
    {
        if (mWindowHandle)
        {
            const_cast<FWindowsPlatformWindow*>(this)->UpdateCachedSizeFromClientRect();
        }
        return mProperties;
    }

    auto             FWindowsPlatformWindow::GetWindowHandle() const noexcept -> void* { return mWindowHandle; }

    LRESULT CALLBACK FWindowsPlatformWindow::WindowProc(
        HWND InWindowHandle, UINT InMessage, WPARAM InWParam, LPARAM InLParam)
    {
        FWindowsPlatformWindow* window = nullptr;

        if (InMessage == WM_NCCREATE)
        {
            const auto* createStruct = reinterpret_cast<CREATESTRUCT*>(InLParam);
            window                   = static_cast<FWindowsPlatformWindow*>(createStruct->lpCreateParams);
            SetWindowLongPtr(InWindowHandle, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(window));
        }
        else
        {
            window = reinterpret_cast<FWindowsPlatformWindow*>(GetWindowLongPtr(InWindowHandle, GWLP_USERDATA));
        }

        if (window)
        {
            window->mWindowHandle = static_cast<void*>(InWindowHandle);
        }

        switch (InMessage)
        {
            case WM_DESTROY:
                PostQuitMessage(0);
                return 0;
            case WM_SIZE:
                if (window)
                {
                    window->UpdateCachedSizeFromClientRect();
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

        WNDCLASS windowClass{};
        windowClass.style         = CS_HREDRAW | CS_VREDRAW;
        windowClass.lpfnWndProc   = &FWindowsPlatformWindow::WindowProc;
        windowClass.hInstance     = static_cast<HINSTANCE>(mInstanceHandle);
        windowClass.lpszClassName = kWindowClassName;
        windowClass.hCursor       = LoadCursor(nullptr, IDC_ARROW);
        windowClass.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);

        if (!RegisterClass(&windowClass))
        {
            const DWORD errorCode = GetLastError();
            if (errorCode != ERROR_CLASS_ALREADY_EXISTS)
            {
                LogError(TEXT("RegisterClass failed (error {})."), errorCode);
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

        RECT clientRect{};
        GetClientRect(static_cast<HWND>(mWindowHandle), &clientRect);
        mCachedSize.mWidth  = static_cast<u32>(clientRect.right - clientRect.left);
        mCachedSize.mHeight = static_cast<u32>(clientRect.bottom - clientRect.top);
        mProperties.mWidth  = mCachedSize.mWidth;
        mProperties.mHeight = mCachedSize.mHeight;
    }

    DWORD FWindowsPlatformWindow::ResolveWindowStyle(const FPlatformWindowProperty& InProperties) const noexcept
    {
        return ToWin32DisplayMode(InProperties.mDisplayMode);
    }

    FWindowsApplication::FWindowsApplication(const FStartupParameters& InStartupParameters)
        : FApplication(InStartupParameters)
    {
    }

    auto FWindowsApplication::CreatePlatformWindow() -> FWindowOwner
    {
        auto window = MakeUnique<FWindowsPlatformWindow>();
        return FWindowOwner(window.release());
    }

    void FWindowsApplication::PumpPlatformMessages()
    {
        MSG message{};
        while (PeekMessage(&message, nullptr, 0, 0, PM_REMOVE))
        {
            if (message.message == WM_QUIT)
            {
                RequestShutdown();
                break;
            }

            TranslateMessage(&message);
            DispatchMessage(&message);
        }
    }

} // namespace AltinaEngine::Application

#endif // AE_PLATFORM_WIN
