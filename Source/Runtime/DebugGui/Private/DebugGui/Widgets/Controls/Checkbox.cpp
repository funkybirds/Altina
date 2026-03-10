#include "DebugGui/Widgets/DebugGuiContext.h"

namespace AltinaEngine::DebugGui::Private {
    auto FDebugGuiContext::Checkbox(FStringView label, bool& value) -> bool {
        const u64       id  = HashId(label);
        const f32       box = mTheme->mCheckboxBoxSize;
        const FRect     boxRect{ mCursor, FVector2f(mCursor.X() + box, mCursor.Y() + box) };
        const FVector2f textPos(mCursor.X() + box + mTheme->mCheckboxTextOffsetX,
            mCursor.Y() + mTheme->mButtonPaddingY);
        const f32       w = box + mTheme->mCheckboxTextOffsetX + CalcTextWidth(label);
        const f32       h = box;
        const FRect     fullRect{ mCursor, FVector2f(mCursor.X() + w, mCursor.Y() + h) };

        const bool      hovered = PointInRect(mInput.mMousePos, fullRect);
        if (hovered) {
            mUi->mHotId             = id;
            mUi->mWantsCaptureMouse = true;
        }

        bool changed = false;
        if (hovered && mInput.mMousePressed) {
            mUi->mActiveId = id;
            mUi->mFocusId  = id;
        }
        if (mUi->mActiveId == id && mInput.mMouseReleased) {
            if (hovered) {
                value   = !value;
                changed = true;
            }
            mUi->mActiveId = 0ULL;
        }

        DrawRectFilled(boxRect, mTheme->mCheckboxBoxBg);
        DrawRect(boxRect, mTheme->mCheckboxBoxBorder, 1.0f);
        if (value) {
            const f32   inset = mTheme->mCheckboxMarkInset;
            const FRect mark{ FVector2f(boxRect.Min.X() + inset, boxRect.Min.Y() + inset),
                FVector2f(boxRect.Max.X() - inset, boxRect.Max.Y() - inset) };
            DrawRectFilled(mark, mTheme->mCheckboxMark);
        }
        DrawText(textPos, mTheme->mText, label);

        AdvanceItem(FVector2f(w, h));
        return changed;
    }
} // namespace AltinaEngine::DebugGui::Private
