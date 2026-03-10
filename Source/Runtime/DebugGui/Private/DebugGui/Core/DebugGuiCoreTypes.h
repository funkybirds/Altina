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
        f32 mX     = 0.0f;
        f32 mY     = 0.0f;
        f32 mU     = 0.0f;
        f32 mV     = 0.0f;
        u32 mColor = 0U; // RGBA8
    };

    struct FDrawCmd {
        u32   mIndexCount  = 0U;
        u32   mIndexOffset = 0U;
        u64   mTextureId   = 0ULL; // 0 = internal font/solid texture.
        FRect mClipRect{};
    };

    struct FDrawData {
        TVector<FDrawVertex> mVertices;
        TVector<u32>         mIndices;
        TVector<FDrawCmd>    mCmds;

        void                 Clear() {
            mVertices.Clear();
            mIndices.Clear();
            mCmds.Clear();
        }
    };

    struct FClipRectStack {
        TVector<FRect> mStack;

        void           Clear() { mStack.Clear(); }
        void           Push(const FRect& r) { mStack.PushBack(r); }
        void           Pop() {
            if (!mStack.IsEmpty()) {
                mStack.PopBack();
            }
        }

        [[nodiscard]] auto Current(const FVector2f& displaySize) const noexcept -> FRect {
            if (mStack.IsEmpty()) {
                return { .Min = FVector2f(0.0f, 0.0f), .Max = displaySize };
            }
            return mStack.Back();
        }
    };

    struct FUIState {
        u64  mHotId                = 0ULL;
        u64  mActiveId             = 0ULL;
        u64  mFocusId              = 0ULL;
        bool mWantsCaptureMouse    = false;
        bool mWantsCaptureKeyboard = false;

        void ClearTransient() noexcept {
            mHotId                = 0ULL;
            mWantsCaptureMouse    = false;
            mWantsCaptureKeyboard = false;
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
        return { .Min = FVector2f(minX, minY), .Max = FVector2f(maxX, maxY) };
    }

    struct FGuiInput {
        const Input::FInputSystem* mInput               = nullptr;
        FVector2f                  mMousePos            = FVector2f(0.0f, 0.0f);
        i32                        mMouseDeltaX         = 0;
        i32                        mMouseDeltaY         = 0;
        bool                       mMouseDown           = false;
        bool                       mMousePressed        = false;
        bool                       mMouseReleased       = false;
        f32                        mMouseWheelDelta     = 0.0f;
        bool                       mKeyEnterPressed     = false;
        bool                       mKeyBackspacePressed = false;
    };

    struct FWindowState {
        bool      mInitialized = false;
        bool      mCollapsed   = false;
        FVector2f mPos         = FVector2f(10.0f, 10.0f);
        FVector2f mSize        = FVector2f(460.0f, 260.0f);
    };
} // namespace AltinaEngine::DebugGui::Private
