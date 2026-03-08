#include "DebugGui/Widgets/DebugGuiContext.h"

namespace AltinaEngine::DebugGui::Private {
    auto FDebugGuiContext::SliderFloat(FStringView label, f32& value, f32 minValue, f32 maxValue)
        -> bool {
        if (maxValue <= minValue) {
            return false;
        }

        Text(label);
        const u64   id = HashId(label);
        const f32   w  = mContentMax.X() - mContentMin.X();
        const f32   h  = mTheme->SliderHeight;
        const FRect r{ mCursor, FVector2f(mCursor.X() + w, mCursor.Y() + h) };

        const bool  hovered = PointInRect(mInput.MousePos, r);
        if (hovered) {
            mUi->HotId              = id;
            mUi->bWantsCaptureMouse = true;
        }

        if (hovered && mInput.bMousePressed) {
            mUi->ActiveId = id;
            mUi->FocusId  = id;
        }

        bool changed = false;
        if (mUi->ActiveId == id && mInput.bMouseDown) {
            const f32 t        = (mInput.MousePos.X() - r.Min.X()) / (r.Max.X() - r.Min.X());
            const f32 tt       = (t < 0.0f) ? 0.0f : ((t > 1.0f) ? 1.0f : t);
            const f32 newValue = minValue + tt * (maxValue - minValue);
            if (newValue != value) {
                value   = newValue;
                changed = true;
            }
        }
        if (mUi->ActiveId == id && mInput.bMouseReleased) {
            mUi->ActiveId = 0ULL;
        }

        DrawRectFilled(r, mTheme->SliderBg);
        DrawRect(r, mTheme->SliderBorder, 1.0f);
        const f32   norm  = (value - minValue) / (maxValue - minValue);
        const f32   fillW = (norm < 0.0f) ? 0.0f : ((norm > 1.0f) ? 1.0f : norm);
        const FRect fill{ r.Min, FVector2f(r.Min.X() + w * fillW, r.Max.Y()) };
        DrawRectFilled(fill, mTheme->SliderFill);

        AdvanceItem(FVector2f(w, h + mTheme->SliderBottomSpacingY));
        return changed;
    }
} // namespace AltinaEngine::DebugGui::Private
