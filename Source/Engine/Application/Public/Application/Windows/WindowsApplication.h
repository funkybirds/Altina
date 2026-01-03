#pragma once

#include "Base/AltinaBase.h"

#if !AE_PLATFORM_WIN
    #error "FWindowsApplication is only available when compiling for Windows platforms."
#endif

#ifndef NOMINMAX
    #define NOMINMAX
#endif

// Forward-declare minimal Windows types to avoid including Windows.h in public headers.
typedef struct HINSTANCE__* HINSTANCE; // NOLINT(*-use-using)
typedef struct HWND__*      HWND;      // NOLINT(*-use-using)
using DWORD   = unsigned long;
using UINT    = unsigned int;
using WPARAM  = unsigned long long;
using LPARAM  = long long;
using LRESULT = long long;

#ifndef CALLBACK
    #define CALLBACK
#endif

#include "Application/Application.h"

namespace AltinaEngine::Application {

    class AE_APPLICATION_API FWindowsPlatformWindow final : public FPlatformWindow {
    public:
        FWindowsPlatformWindow();
        ~FWindowsPlatformWindow() override;

        auto               Initialize(const FPlatformWindowProperty& InProperties) -> bool override;
        void               Show() override;
        void               Hide() override;
        void               Resize(u32 InWidth, u32 InHeight) override;
        void               MoveTo(i32 InPositionX, i32 InPositionY) override;
        void               Minimalize() override;
        void               Maximalize() override;

        [[nodiscard]] auto GetSize() const noexcept -> FWindowExtent override;
        [[nodiscard]] auto GetProperties() const -> FPlatformWindowProperty override;
        [[nodiscard]] auto GetWindowHandle() const noexcept -> void*;

    private:
        static LRESULT CALLBACK WindowProc(
            HWND InWindowHandle, UINT InMessage, WPARAM InWParam, LPARAM InLParam);

        void               RegisterWindowClass();
        void               UpdateCachedSizeFromClientRect();
        [[nodiscard]] auto ResolveWindowStyle(
            const FPlatformWindowProperty& InProperties) const noexcept -> DWORD;

        void*                   mWindowHandle   = nullptr;
        void*                   mInstanceHandle = nullptr;
        FPlatformWindowProperty mProperties{};
        FWindowExtent           mCachedSize{};
    };

    class AE_APPLICATION_API FWindowsApplication final : public FApplication {
    public:
        explicit FWindowsApplication(const FStartupParameters& InStartupParameters);
        ~FWindowsApplication() override = default;

    protected:
        auto CreatePlatformWindow() -> FWindowOwner override;
        void PumpPlatformMessages() override;
    };

} // namespace AltinaEngine::Application
