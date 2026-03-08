#include "DebugGui/Widgets/DebugGuiContext.h"

namespace AltinaEngine::DebugGui::Private {
    auto FDebugGuiContext::Button(FStringView label) -> bool {
        const u64       id   = HashId(label);
        const FVector2f size = CalcButtonSize(label);
        const FRect     r{ mCursor, FVector2f(mCursor.X() + size.X(), mCursor.Y() + size.Y()) };

        const bool      hovered = PointInRect(mInput.MousePos, r);
        if (hovered) {
            mUi->HotId              = id;
            mUi->bWantsCaptureMouse = true;
        }

        bool pressed = false;
        if (hovered && mInput.bMousePressed) {
            mUi->ActiveId = id;
            mUi->FocusId  = id;
        }
        if (mUi->ActiveId == id) {
            if (mInput.bMouseReleased) {
                if (hovered) {
                    pressed = true;
                }
                mUi->ActiveId = 0ULL;
            }
        }

        FColor32 bg = hovered ? mTheme->ButtonHoveredBg : mTheme->ButtonBg;
        if (mUi->ActiveId == id) {
            bg = mTheme->ButtonActiveBg;
        }
        DrawRectFilled(r, bg);
        DrawRect(r, mTheme->ButtonBorder, 1.0f);

        const FVector2f textPos(
            r.Min.X() + mTheme->ButtonPaddingX, r.Min.Y() + mTheme->ButtonPaddingY);
        DrawText(textPos, mTheme->ButtonText, label);

        AdvanceItem(size);
        return pressed;
    }
} // namespace AltinaEngine::DebugGui::Private
