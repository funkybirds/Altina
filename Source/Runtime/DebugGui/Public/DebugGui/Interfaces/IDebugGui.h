#pragma once

#include "DebugGui/DebugGuiAPI.h"
#include "Container/String.h"
#include "DebugGui/Core/Theme.h"

namespace AltinaEngine::DebugGui {
    namespace Container = Core::Container;

    class AE_DEBUGGUI_API IDebugGui {
    public:
        virtual ~IDebugGui() = default;

        // Low-level drawing.
        virtual void PushClipRect(const FRect& rect) = 0;
        virtual void PopClipRect()                   = 0;

        virtual void DrawRectFilled(const FRect& rect, FColor32 color)                      = 0;
        virtual void DrawRect(const FRect& rect, FColor32 color, f32 thickness)             = 0;
        virtual void DrawRoundedRectFilled(const FRect& rect, FColor32 color, f32 rounding) = 0;
        virtual void DrawRoundedRect(
            const FRect& rect, FColor32 color, f32 rounding, f32 thickness) = 0;
        virtual void DrawCapsuleFilled(
            const FVector2f& a, const FVector2f& b, f32 radius, FColor32 color) = 0;
        virtual void DrawCapsule(
            const FVector2f& a, const FVector2f& b, f32 radius, FColor32 color, f32 thickness) = 0;
        virtual void DrawLine(
            const FVector2f& p0, const FVector2f& p1, FColor32 color, f32 thickness) = 0;
        virtual void DrawTriangleFilled(
            const FVector2f& p0, const FVector2f& p1, const FVector2f& p2, FColor32 color) = 0;
        virtual void DrawText(const FVector2f& pos, FColor32 color, FStringView text)      = 0;

        [[nodiscard]] virtual auto GetDisplaySize() const noexcept -> FVector2f = 0;
        [[nodiscard]] virtual auto GetMousePos() const noexcept -> FVector2f    = 0;

        // Widgets.
        virtual bool               BeginWindow(FStringView title, bool* open = nullptr) = 0;
        virtual void               EndWindow()                                          = 0;

        virtual void               Text(FStringView text) = 0;
        virtual void               Separator()            = 0;

        [[nodiscard]] virtual bool Button(FStringView label)                = 0;
        [[nodiscard]] virtual bool Checkbox(FStringView label, bool& value) = 0;
        [[nodiscard]] virtual bool SliderFloat(
            FStringView label, f32& value, f32 minValue, f32 maxValue)                     = 0;
        [[nodiscard]] virtual bool InputText(FStringView label, Container::FString& value) = 0;
    };
} // namespace AltinaEngine::DebugGui
