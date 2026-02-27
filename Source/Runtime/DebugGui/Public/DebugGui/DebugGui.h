#pragma once

#include "DebugGui/DebugGuiAPI.h"

#include "Container/Function.h"
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

    class AE_DEBUGGUI_API IDebugGui {
    public:
        virtual ~IDebugGui() = default;

        // Low-level drawing (Phase 1).
        virtual void PushClipRect(const FRect& rect) = 0;
        virtual void PopClipRect()                   = 0;

        virtual void DrawRectFilled(const FRect& rect, FColor32 color)          = 0;
        virtual void DrawRect(const FRect& rect, FColor32 color, f32 thickness) = 0;
        virtual void DrawLine(
            const FVector2f& p0, const FVector2f& p1, FColor32 color, f32 thickness)  = 0;
        virtual void DrawText(const FVector2f& pos, FColor32 color, FStringView text) = 0;

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

        virtual void               RegisterPanel(FStringView name, FPanelFn fn) = 0;

        virtual void SetExternalStats(const FDebugGuiExternalStats& stats) noexcept = 0;

        virtual void TickGameThread(const Input::FInputSystem& input, f32 dtSeconds,
            u32 displayWidth, u32 displayHeight)                                              = 0;
        virtual void RenderRenderThread(Rhi::FRhiDevice& device, Rhi::FRhiViewport& viewport) = 0;

        [[nodiscard]] virtual auto WantsCaptureKeyboard() const noexcept -> bool = 0;
        [[nodiscard]] virtual auto WantsCaptureMouse() const noexcept -> bool    = 0;

        [[nodiscard]] virtual auto GetLastFrameStats() const noexcept -> FDebugGuiFrameStats = 0;
    };

    // Factory helpers (to avoid exposing concrete types in other module headers).
    AE_DEBUGGUI_API auto CreateDebugGuiSystem() -> IDebugGuiSystem*;
    AE_DEBUGGUI_API void DestroyDebugGuiSystem(IDebugGuiSystem* sys);
} // namespace AltinaEngine::DebugGui
