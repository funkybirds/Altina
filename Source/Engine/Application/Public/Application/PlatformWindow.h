#pragma once

#include "Base/ApplicationAPI.h"
#include "CoreMinimal.h"
#include "Container/String.h"

namespace AltinaEngine::Application
{
    using AltinaEngine::Core::Container::FString;

    enum class EWindowDisplayMode : u8
    {
        Windowed = 0,
        Fullscreen,
        Borderless
    };

    struct AE_APPLICATION_API FWindowExtent
    {
        u32 mWidth  = 0;
        u32 mHeight = 0;
    };

    struct AE_APPLICATION_API FPlatformWindowProperty
    {
        FString            mTitle       = FString(TEXT("AltinaEngine"));
        u32                mWidth       = 1280;
        u32                mHeight      = 720;
        f32                mDpiScaling  = 1.0f;
        EWindowDisplayMode mDisplayMode = EWindowDisplayMode::Windowed;
    };

    class AE_APPLICATION_API FPlatformWindow
    {
    public:
        virtual ~FPlatformWindow() = default;

        virtual auto               Initialize(const FPlatformWindowProperty& InProperties) -> bool = 0;
        virtual void               Show()                                                          = 0;
        virtual void               Hide()                                                          = 0;
        virtual void               Resize(u32 InWidth, u32 InHeight)                               = 0;
        virtual void               MoveTo(i32 InPositionX, i32 InPositionY)                        = 0;
        virtual void               Minimalize()                                                    = 0;
        virtual void               Maximalize()                                                    = 0;

        [[nodiscard]] virtual auto GetSize() const noexcept -> FWindowExtent        = 0;
        [[nodiscard]] virtual auto GetProperties() const -> FPlatformWindowProperty = 0;
    };

} // namespace AltinaEngine::Application
