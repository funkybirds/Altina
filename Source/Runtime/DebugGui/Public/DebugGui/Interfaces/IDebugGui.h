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

        virtual void DrawRectFilled(const FRect& rect, FColor32 color)          = 0;
        virtual void DrawRect(const FRect& rect, FColor32 color, f32 thickness) = 0;
        virtual void DrawRoundedRectFilled(const FRect& rect, FColor32 color, f32 rounding,
            EDebugGuiCornerFlags cornerFlags = EDebugGuiCornerFlags::All)       = 0;
        virtual void DrawRoundedRect(const FRect& rect, FColor32 color, f32 rounding, f32 thickness,
            EDebugGuiCornerFlags cornerFlags = EDebugGuiCornerFlags::All)       = 0;
        virtual void DrawCapsuleFilled(
            const FVector2f& a, const FVector2f& b, f32 radius, FColor32 color) = 0;
        virtual void DrawCapsule(
            const FVector2f& a, const FVector2f& b, f32 radius, FColor32 color, f32 thickness) = 0;
        virtual void DrawLine(
            const FVector2f& p0, const FVector2f& p1, FColor32 color, f32 thickness) = 0;
        virtual void DrawTriangleFilled(
            const FVector2f& p0, const FVector2f& p1, const FVector2f& p2, FColor32 color) = 0;
        virtual void DrawText(const FVector2f& pos, FColor32 color, FStringView text)      = 0;
        virtual void DrawTextStyled(
            const FVector2f& pos, FColor32 color, FStringView text, EDebugGuiFontRole role) = 0;
        virtual void               DrawImage(const FRect& rect, u64 imageId, FColor32 tint) = 0;
        [[nodiscard]] virtual auto MeasureText(FStringView text, EDebugGuiFontRole role) const
            -> FVector2f = 0;

        [[nodiscard]] virtual auto GetDisplaySize() const noexcept -> FVector2f = 0;
        [[nodiscard]] virtual auto GetMousePos() const noexcept -> FVector2f    = 0;
        [[nodiscard]] virtual auto GetCursorPos() const noexcept -> FVector2f   = 0;
        virtual void               SetCursorPos(const FVector2f& p) noexcept    = 0;
        [[nodiscard]] virtual auto IsMouseDown() const noexcept -> bool         = 0;
        [[nodiscard]] virtual auto WasMousePressed() const noexcept -> bool     = 0;
        [[nodiscard]] virtual auto WasMouseReleased() const noexcept -> bool    = 0;

        // Widgets.
        virtual auto               BeginWindow(FStringView title, bool* open) -> bool = 0;
        virtual void               EndWindow()                                        = 0;

        virtual void               Text(FStringView text) = 0;
        virtual void               Separator()            = 0;

        [[nodiscard]] virtual auto Button(FStringView label) -> bool                = 0;
        [[nodiscard]] virtual auto Checkbox(FStringView label, bool& value) -> bool = 0;
        [[nodiscard]] virtual auto SliderFloat(
            FStringView label, f32& value, f32 minValue, f32 maxValue) -> bool = 0;
        [[nodiscard]] virtual auto InputText(FStringView label, Container::FString& value)
            -> bool                                                                   = 0;
        [[nodiscard]] virtual auto Gizmo(FStringView label, FVector2f& value) -> bool = 0;
        [[nodiscard]] virtual auto TreeViewItem(const FTreeViewItemDesc& desc)
            -> FTreeViewItemResult = 0;
        [[nodiscard]] virtual auto TextedIconView(const FTextedIconViewDesc& desc)
            -> FTextedIconViewResult = 0;
    };
} // namespace AltinaEngine::DebugGui
