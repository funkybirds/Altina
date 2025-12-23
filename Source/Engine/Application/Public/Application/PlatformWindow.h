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
        u32 Width  = 0;
        u32 Height = 0;
    };

    struct AE_APPLICATION_API FPlatformWindowProperty
    {
        FString             Title       = FString(TEXT("AltinaEngine"));
        u32                 Width       = 1280;
        u32                 Height      = 720;
        f32                 DPIScaling  = 1.0f;
        EWindowDisplayMode  DisplayMode = EWindowDisplayMode::Windowed;
    };

    class AE_APPLICATION_API FPlatformWindow
    {
    public:
        virtual ~FPlatformWindow() = default;

        virtual bool Initialize(const FPlatformWindowProperty& InProperties) = 0;
        virtual void Show()        = 0;
        virtual void Hide()        = 0;
        virtual void Resize(u32 InWidth, u32 InHeight)                       = 0;
        virtual void MoveTo(i32 InPositionX, i32 InPositionY)                 = 0;
        virtual void Minimalize()                                             = 0;
        virtual void Maximalize()                                             = 0;

        [[nodiscard]] virtual FWindowExtent GetSize() const noexcept          = 0;
        [[nodiscard]] virtual FPlatformWindowProperty GetProperties() const   = 0;
    };

} // namespace AltinaEngine::Application
