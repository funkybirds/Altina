#include "DebugGui/Widgets/DebugGuiContext.h"

namespace AltinaEngine::DebugGui::Private {
    auto FDebugGuiContext::Checkbox(FStringView label, bool& value) -> bool {
        const u64       id  = HashId(label);
        const f32       box = mTheme->CheckboxBoxSize;
        const FRect     boxRect{ mCursor, FVector2f(mCursor.X() + box, mCursor.Y() + box) };
        const FVector2f textPos(
            mCursor.X() + box + mTheme->CheckboxTextOffsetX, mCursor.Y() + mTheme->ButtonPaddingY);
        const f32   w = box + mTheme->CheckboxTextOffsetX + CalcTextWidth(label);
        const f32   h = box;
        const FRect fullRect{ mCursor, FVector2f(mCursor.X() + w, mCursor.Y() + h) };

        const bool  hovered = PointInRect(mInput.MousePos, fullRect);
        if (hovered) {
            mUi->HotId              = id;
            mUi->bWantsCaptureMouse = true;
        }

        bool changed = false;
        if (hovered && mInput.bMousePressed) {
            mUi->ActiveId = id;
            mUi->FocusId  = id;
        }
        if (mUi->ActiveId == id && mInput.bMouseReleased) {
            if (hovered) {
                value   = !value;
                changed = true;
            }
            mUi->ActiveId = 0ULL;
        }

        DrawRectFilled(boxRect, mTheme->CheckboxBoxBg);
        DrawRect(boxRect, mTheme->CheckboxBoxBorder, 1.0f);
        if (value) {
            const f32   inset = mTheme->CheckboxMarkInset;
            const FRect mark{ FVector2f(boxRect.Min.X() + inset, boxRect.Min.Y() + inset),
                FVector2f(boxRect.Max.X() - inset, boxRect.Max.Y() - inset) };
            DrawRectFilled(mark, mTheme->CheckboxMark);
        }
        DrawText(textPos, mTheme->Text, label);

        AdvanceItem(FVector2f(w, h));
        return changed;
    }
} // namespace AltinaEngine::DebugGui::Private
