#pragma once

#include "DebugGui/DebugGui.h"

#include "Container/HashMap.h"
#include "Container/String.h"
#include "Container/Vector.h"

namespace AltinaEngine::DebugGui::Private {
    namespace Container = Core::Container;
    using Container::FString;
    using Container::TVector;
    using Core::Math::FVector2f;

    struct FDrawVertex {
        f32 X     = 0.0f;
        f32 Y     = 0.0f;
        f32 U     = 0.0f;
        f32 V     = 0.0f;
        u32 Color = 0U; // RGBA8
    };

    struct FDrawCmd {
        u32   IndexCount  = 0U;
        u32   IndexOffset = 0U;
        FRect ClipRect{};
    };

    struct FDrawData {
        TVector<FDrawVertex> Vertices;
        TVector<u32>         Indices;
        TVector<FDrawCmd>    Cmds;

        void                 Clear() {
            Vertices.Clear();
            Indices.Clear();
            Cmds.Clear();
        }
    };

    struct FClipRectStack {
        TVector<FRect> Stack;

        void           Clear() { Stack.Clear(); }
        void           Push(const FRect& r) { Stack.PushBack(r); }
        void           Pop() {
            if (!Stack.IsEmpty()) {
                Stack.PopBack();
            }
        }

        [[nodiscard]] auto Current(const FVector2f& displaySize) const noexcept -> FRect {
            if (Stack.IsEmpty()) {
                return { FVector2f(0.0f, 0.0f), displaySize };
            }
            return Stack.Back();
        }
    };

    struct FUIState {
        u64  HotId                 = 0ULL;
        u64  ActiveId              = 0ULL;
        u64  FocusId               = 0ULL;
        bool bWantsCaptureMouse    = false;
        bool bWantsCaptureKeyboard = false;

        void ClearTransient() noexcept {
            HotId                 = 0ULL;
            bWantsCaptureMouse    = false;
            bWantsCaptureKeyboard = false;
        }
    };

    [[nodiscard]] inline auto PointInRect(const FVector2f& p, const FRect& r) noexcept -> bool {
        return p.X() >= r.Min.X() && p.X() < r.Max.X() && p.Y() >= r.Min.Y() && p.Y() < r.Max.Y();
    }

    [[nodiscard]] inline auto IntersectRect(const FRect& a, const FRect& b) noexcept -> FRect {
        const f32 minX = (a.Min.X() > b.Min.X()) ? a.Min.X() : b.Min.X();
        const f32 minY = (a.Min.Y() > b.Min.Y()) ? a.Min.Y() : b.Min.Y();
        const f32 maxX = (a.Max.X() < b.Max.X()) ? a.Max.X() : b.Max.X();
        const f32 maxY = (a.Max.Y() < b.Max.Y()) ? a.Max.Y() : b.Max.Y();
        return { FVector2f(minX, minY), FVector2f(maxX, maxY) };
    }

    struct FGuiInput {
        const Input::FInputSystem* Input                = nullptr;
        FVector2f                  MousePos             = FVector2f(0.0f, 0.0f);
        bool                       bMouseDown           = false;
        bool                       bMousePressed        = false;
        bool                       bMouseReleased       = false;
        f32                        MouseWheelDelta      = 0.0f;
        bool                       bKeyEnterPressed     = false;
        bool                       bKeyBackspacePressed = false;
    };

    struct FWindowState {
        bool      bInitialized = false;
        bool      bCollapsed   = false;
        FVector2f Pos          = FVector2f(10.0f, 10.0f);
        FVector2f Size         = FVector2f(460.0f, 260.0f);
    };
} // namespace AltinaEngine::DebugGui::Private
