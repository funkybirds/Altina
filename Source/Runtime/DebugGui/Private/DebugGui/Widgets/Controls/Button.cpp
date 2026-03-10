#include "DebugGui/Widgets/DebugGuiContext.h"

namespace AltinaEngine::DebugGui::Private {
    auto FDebugGuiContext::Button(FStringView label) -> bool {
        const u64       id   = HashId(label);
        const FVector2f size = CalcButtonSize(label);
        const FRect     r{ mCursor, FVector2f(mCursor.X() + size.X(), mCursor.Y() + size.Y()) };

        const bool      hovered = PointInRect(mInput.mMousePos, r);
        if (hovered) {
            mUi->mHotId             = id;
            mUi->mWantsCaptureMouse = true;
        }

        bool pressed = false;
        if (hovered && mInput.mMousePressed) {
            mUi->mActiveId = id;
            mUi->mFocusId  = id;
        }
        if (mUi->mActiveId == id) {
            if (mInput.mMouseReleased) {
                if (hovered) {
                    pressed = true;
                }
                mUi->mActiveId = 0ULL;
            }
        }

        FColor32 bg = hovered ? mTheme->mButtonHoveredBg : mTheme->mButtonBg;
        if (mUi->mActiveId == id) {
            bg = mTheme->mButtonActiveBg;
        }
        DrawRectFilled(r, bg);
        DrawRect(r, mTheme->mButtonBorder, 1.0f);

        const FVector2f textPos(
            r.Min.X() + mTheme->mButtonPaddingX, r.Min.Y() + mTheme->mButtonPaddingY);
        DrawText(textPos, mTheme->mButtonText, label);

        AdvanceItem(size);
        return pressed;
    }
} // namespace AltinaEngine::DebugGui::Private
