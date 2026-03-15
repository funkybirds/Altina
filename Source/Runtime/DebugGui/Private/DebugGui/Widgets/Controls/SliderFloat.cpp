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
        const f32   h  = mTheme->mSliderHeight;
        const FRect r{ mCursor, FVector2f(mCursor.X() + w, mCursor.Y() + h) };

        const bool  hovered = PointInRect(mInput.mMousePos, r);
        if (hovered) {
            mUi->mHotId             = id;
            mUi->mWantsCaptureMouse = true;
        }

        if (hovered && mInput.mMousePressed) {
            mUi->mActiveId = id;
            mUi->mFocusId  = id;
        }

        bool changed = false;
        if (mUi->mActiveId == id && mInput.mMouseDown) {
            const f32 t        = (mInput.mMousePos.X() - r.Min.X()) / (r.Max.X() - r.Min.X());
            const f32 tt       = (t < 0.0f) ? 0.0f : ((t > 1.0f) ? 1.0f : t);
            const f32 newValue = minValue + tt * (maxValue - minValue);
            if (newValue != value) {
                value   = newValue;
                changed = true;
            }
        }
        if (mUi->mActiveId == id && mInput.mMouseReleased) {
            mUi->mActiveId = 0ULL;
        }

        const f32 rounding = mTheme->mEditor.mPanelSurface.mCornerRadius;
        DrawRoundedRectFilled(r, mTheme->mSliderBg, rounding);
        if ((mTheme->mSliderBorder >> 24U) != 0U) {
            DrawRoundedRect(r, mTheme->mSliderBorder, rounding, 1.0f);
        }
        const f32   norm  = (value - minValue) / (maxValue - minValue);
        const f32   fillW = (norm < 0.0f) ? 0.0f : ((norm > 1.0f) ? 1.0f : norm);
        const FRect fill{ r.Min, FVector2f(r.Min.X() + w * fillW, r.Max.Y()) };
        DrawRoundedRectFilled(fill, mTheme->mSliderFill, rounding);

        AdvanceItem(FVector2f(w, h + mTheme->mSliderBottomSpacingY));
        return changed;
    }
} // namespace AltinaEngine::DebugGui::Private
