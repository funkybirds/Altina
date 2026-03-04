#pragma once

#include "DebugGui/DebugGuiAPI.h"

#include "Container/Function.h"
#include "Container/SmartPtr.h"
#include "Container/String.h"
#include "Container/StringView.h"
#include "Math/Vector.h"
#include "Types/Aliases.h"

namespace AltinaEngine::Input {
    class FInputSystem;
}

namespace AltinaEngine::Rhi {
    class FRhiDevice;
    class FRhiViewport;
} // namespace AltinaEngine::Rhi

namespace AltinaEngine::DebugGui {
    namespace Container = Core::Container;
    using Container::FStringView;
    using Container::TFunction;
    using Container::TOwner;
    using Container::TPolymorphicDeleter;

    using Core::Math::FVector2f;

    struct FRect {
        FVector2f Min = FVector2f(0.0f, 0.0f);
        FVector2f Max = FVector2f(0.0f, 0.0f);
    };

    // Packed RGBA8 in little-endian memory order: R | (G<<8) | (B<<16) | (A<<24).
    using FColor32 = u32;

    [[nodiscard]] constexpr auto MakeColor32(u8 r, u8 g, u8 b, u8 a = 255) noexcept -> FColor32 {
        return static_cast<FColor32>(r) | (static_cast<FColor32>(g) << 8U)
            | (static_cast<FColor32>(b) << 16U) | (static_cast<FColor32>(a) << 24U);
    }

    struct FDebugGuiTheme {
        // Colors.
        FColor32  WindowBg     = MakeColor32(15, 15, 15, 200);
        FColor32  WindowBorder = MakeColor32(255, 255, 255, 140);
        FColor32  TitleBarBg   = MakeColor32(25, 25, 25, 220);
        FColor32  Text         = MakeColor32(220, 220, 220, 255);
        FColor32  TitleText    = MakeColor32(255, 255, 255, 255);

        FColor32  Separator = MakeColor32(255, 255, 255, 80);

        FColor32  ButtonBg        = MakeColor32(60, 60, 60, 220);
        FColor32  ButtonHoveredBg = MakeColor32(80, 80, 80, 220);
        FColor32  ButtonActiveBg  = MakeColor32(100, 100, 100, 240);
        FColor32  ButtonBorder    = MakeColor32(255, 255, 255, 100);
        FColor32  ButtonText      = MakeColor32(255, 255, 255, 255);

        FColor32  CheckboxBoxBg     = MakeColor32(30, 30, 30, 255);
        FColor32  CheckboxBoxBorder = MakeColor32(255, 255, 255, 120);
        FColor32  CheckboxMark      = MakeColor32(140, 200, 140, 255);

        FColor32  SliderBg     = MakeColor32(40, 40, 40, 255);
        FColor32  SliderBorder = MakeColor32(255, 255, 255, 90);
        FColor32  SliderFill   = MakeColor32(120, 160, 220, 255);

        FColor32  InputBg           = MakeColor32(25, 25, 25, 255);
        FColor32  InputActiveBg     = MakeColor32(30, 30, 30, 255);
        FColor32  InputBorder       = MakeColor32(255, 255, 255, 90);
        FColor32  InputActiveBorder = MakeColor32(255, 255, 255, 160);
        FColor32  InputText         = MakeColor32(220, 220, 220, 255);

        FColor32  ScrollBarTrackBg       = MakeColor32(10, 10, 10, 90);
        FColor32  ScrollBarTrackBorder   = MakeColor32(255, 255, 255, 60);
        FColor32  ScrollBarThumbBg       = MakeColor32(110, 110, 110, 180);
        FColor32  ScrollBarThumbHoverBg  = MakeColor32(140, 140, 140, 200);
        FColor32  ScrollBarThumbActiveBg = MakeColor32(160, 160, 160, 220);
        FColor32  ScrollBarThumbBorder   = MakeColor32(255, 255, 255, 80);

        FColor32  CollapseButtonBg       = MakeColor32(45, 45, 45, 160);
        FColor32  CollapseButtonHoverBg  = MakeColor32(60, 60, 60, 200);
        FColor32  CollapseButtonActiveBg = MakeColor32(80, 80, 80, 220);
        FColor32  CollapseButtonBorder   = MakeColor32(255, 255, 255, 90);
        FColor32  CollapseIcon           = MakeColor32(240, 240, 240, 255);

        FColor32  SelectedRowBg = MakeColor32(60, 80, 110, 220);
        FColor32  HoveredRowBg  = MakeColor32(55, 55, 55, 200);

        // Metrics.
        FVector2f WindowDefaultSize = FVector2f(460.0f, 260.0f);
        FVector2f WindowDefaultPos  = FVector2f(10.0f, 10.0f);
        f32       WindowPadding     = 8.0f;
        f32       WindowSpacing     = 10.0f;
        f32       TitleBarHeight    = 18.0f;
        f32       TitleTextOffsetY  = 4.0f;

        f32       SeparatorPaddingY = 4.0f;
        f32       ItemSpacingY      = 4.0f;

        f32       ButtonPaddingX = 6.0f;
        f32       ButtonPaddingY = 3.0f;

        f32       CheckboxBoxSize     = 14.0f;
        f32       CheckboxTextOffsetX = 8.0f;
        f32       CheckboxMarkInset   = 3.0f;

        f32       SliderHeight         = 16.0f;
        f32       SliderBottomSpacingY = 4.0f;

        f32       InputHeight         = 18.0f;
        f32       InputTextOffsetX    = 6.0f;
        f32       InputTextOffsetY    = 4.0f;
        f32       InputBottomSpacingY = 6.0f;

        f32       ScrollBarWidth          = 10.0f;
        f32       ScrollBarPadding        = 2.0f;
        f32       ScrollBarThumbMinHeight = 14.0f;

        f32       CollapseButtonSize     = 12.0f;
        f32       CollapseButtonPadX     = 6.0f;
        f32       CollapseButtonOffsetY  = 3.0f;
        f32       CollapseIconHalfWidth  = 4.0f;
        f32       CollapseIconHalfHeight = 3.0f;
    };

    class AE_DEBUGGUI_API IDebugGui {
    public:
        virtual ~IDebugGui() = default;

        // Low-level drawing (Phase 1).
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

        // Widgets (Phase 2).
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

    using FPanelFn = TFunction<void(IDebugGui&)>;

    struct FDebugGuiFrameStats {
        u32 VertexCount = 0U;
        u32 IndexCount  = 0U;
        u32 CmdCount    = 0U;
    };

    struct FDebugGuiExternalStats {
        u64 FrameIndex      = 0ULL;
        u32 ViewCount       = 0U;
        u32 SceneBatchCount = 0U;
    };

    class AE_DEBUGGUI_API IDebugGuiSystem {
    public:
        virtual ~IDebugGuiSystem() = default;

        virtual void               SetEnabled(bool enabled) noexcept  = 0;
        [[nodiscard]] virtual auto IsEnabled() const noexcept -> bool = 0;

        // Built-in panels visibility. (Useful for tests and for host applications that want to
        // control which default windows are present.)
        virtual void               SetShowStats(bool show) noexcept   = 0;
        virtual void               SetShowConsole(bool show) noexcept = 0;
        virtual void               SetShowCVars(bool show) noexcept   = 0;

        [[nodiscard]] virtual auto IsStatsShown() const noexcept -> bool   = 0;
        [[nodiscard]] virtual auto IsConsoleShown() const noexcept -> bool = 0;
        [[nodiscard]] virtual auto IsCVarsShown() const noexcept -> bool   = 0;

        virtual void               RegisterPanel(FStringView name, FPanelFn fn) = 0;
        // Overlay panels are rendered without window chrome. Prefer IDebugGui::DrawText and other
        // low-level draw calls (no widgets) so they do not capture input.
        virtual void               RegisterOverlay(FStringView name, FPanelFn fn) = 0;

        virtual void SetExternalStats(const FDebugGuiExternalStats& stats) noexcept = 0;
        virtual void SetTheme(const FDebugGuiTheme& theme) noexcept                 = 0;
        [[nodiscard]] virtual auto GetTheme() const noexcept -> FDebugGuiTheme      = 0;

        virtual void               TickGameThread(const Input::FInputSystem& input, f32 dtSeconds,
                          u32 displayWidth, u32 displayHeight)                                = 0;
        virtual void RenderRenderThread(Rhi::FRhiDevice& device, Rhi::FRhiViewport& viewport) = 0;

        [[nodiscard]] virtual auto WantsCaptureKeyboard() const noexcept -> bool = 0;
        [[nodiscard]] virtual auto WantsCaptureMouse() const noexcept -> bool    = 0;

        [[nodiscard]] virtual auto GetLastFrameStats() const noexcept -> FDebugGuiFrameStats = 0;
    };

    using FDebugGuiSystemOwner = TOwner<IDebugGuiSystem, TPolymorphicDeleter<IDebugGuiSystem>>;

    // Factory helpers (to avoid exposing concrete types in other module headers).
    AE_DEBUGGUI_API auto CreateDebugGuiSystemOwner() -> FDebugGuiSystemOwner;
    AE_DEBUGGUI_API auto CreateDebugGuiSystem() -> IDebugGuiSystem*;
    AE_DEBUGGUI_API void DestroyDebugGuiSystem(IDebugGuiSystem* sys);
} // namespace AltinaEngine::DebugGui
