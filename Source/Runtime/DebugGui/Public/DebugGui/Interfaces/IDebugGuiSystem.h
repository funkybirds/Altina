#pragma once

#include "DebugGui/DebugGuiAPI.h"
#include "Container/Function.h"
#include "DebugGui/Interfaces/IDebugGui.h"

namespace AltinaEngine::Input {
    class FInputSystem;
}

namespace AltinaEngine::Rhi {
    class FRhiDevice;
    class FRhiViewport;
} // namespace AltinaEngine::Rhi

namespace AltinaEngine::DebugGui {
    using Core::Container::TFunction;
    using FPanelFn = TFunction<void(IDebugGui&)>;

    class AE_DEBUGGUI_API IDebugGuiSystem {
    public:
        virtual ~IDebugGuiSystem() = default;

        virtual void               SetEnabled(bool enabled) noexcept  = 0;
        [[nodiscard]] virtual auto IsEnabled() const noexcept -> bool = 0;

        virtual void               SetShowStats(bool show) noexcept   = 0;
        virtual void               SetShowConsole(bool show) noexcept = 0;
        virtual void               SetShowCVars(bool show) noexcept   = 0;

        [[nodiscard]] virtual auto IsStatsShown() const noexcept -> bool   = 0;
        [[nodiscard]] virtual auto IsConsoleShown() const noexcept -> bool = 0;
        [[nodiscard]] virtual auto IsCVarsShown() const noexcept -> bool   = 0;

        virtual void               RegisterPanel(FStringView name, FPanelFn fn)   = 0;
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
} // namespace AltinaEngine::DebugGui
