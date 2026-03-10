#include "DebugGui/Widgets/DebugGuiContext.h"
#include "Math/Geometry2D.h"

namespace AltinaEngine::DebugGui::Private {
    auto FDebugGuiContext::Gizmo(FStringView label, FVector2f& value) -> bool {
        Text(label);

        const f32 availableW = mContentMax.X() - mContentMin.X();
        f32       size       = mTheme->mGizmoSize;
        if (size > availableW) {
            size = availableW;
        }
        if (size < 40.0f) {
            size = 40.0f;
        }

        const FRect     r{ mCursor, FVector2f(mCursor.X() + size, mCursor.Y() + size) };
        const f32       centerHalf = mTheme->mGizmoCenterHalfSize;
        const f32       axisPad    = mTheme->mGizmoPadding;
        const f32       axisLenRaw = size * 0.5f - axisPad;
        const f32       axisLen    = (axisLenRaw > 8.0f) ? axisLenRaw : 8.0f;

        const FVector2f center((r.Min.X() + r.Max.X()) * 0.5f, (r.Min.Y() + r.Max.Y()) * 0.5f);
        const FVector2f xEnd(center.X() + axisLen, center.Y());
        const FVector2f yEnd(center.X(), center.Y() - axisLen);

        const FRect     centerRect{ FVector2f(center.X() - centerHalf, center.Y() - centerHalf),
            FVector2f(center.X() + centerHalf, center.Y() + centerHalf) };

        const u64       baseId = HashId(label);
        const u64       idX    = baseId ^ 0xB339D7E68E5F6A11ULL;
        const u64       idY    = baseId ^ 0x63E6D6D7CC5AAE97ULL;
        const u64       idXY   = baseId ^ 0x58A4D1B2FE0C7C43ULL;

        const f32       hitR      = mTheme->mGizmoHitRadius;
        const f32       hitR2     = hitR * hitR;
        const bool      hoveredXY = PointInRect(mInput.mMousePos, centerRect);
        const f32       distXSq   = DistPointSegmentSq(mInput.mMousePos, center, xEnd);
        const f32       distYSq   = DistPointSegmentSq(mInput.mMousePos, center, yEnd);
        const bool      hoveredX  = distXSq <= hitR2;
        const bool      hoveredY  = distYSq <= hitR2;

        u64             hoveredId = 0ULL;
        if (hoveredXY) {
            hoveredId = idXY;
        } else if (hoveredX && hoveredY) {
            hoveredId = (distXSq <= distYSq) ? idX : idY;
        } else if (hoveredX) {
            hoveredId = idX;
        } else if (hoveredY) {
            hoveredId = idY;
        }

        if (hoveredId != 0ULL) {
            mUi->mHotId             = hoveredId;
            mUi->mWantsCaptureMouse = true;
        }

        if (hoveredId != 0ULL && mInput.mMousePressed) {
            mUi->mActiveId = hoveredId;
            mUi->mFocusId  = hoveredId;
        }

        const bool activeX  = (mUi->mActiveId == idX);
        const bool activeY  = (mUi->mActiveId == idY);
        const bool activeXY = (mUi->mActiveId == idXY);
        const bool active   = activeX || activeY || activeXY;

        bool       changed = false;
        if (active && mInput.mMouseDown) {
            const f32 dx = static_cast<f32>(mInput.mMouseDeltaX) * mTheme->mGizmoDragSensitivity;
            const f32 dy = static_cast<f32>(mInput.mMouseDeltaY) * mTheme->mGizmoDragSensitivity;

            if ((activeX || activeXY) && dx != 0.0f) {
                value.X() += dx;
                changed = true;
            }
            if ((activeY || activeXY) && dy != 0.0f) {
                // Screen-space Y grows downward; invert to get conventional gizmo semantics.
                value.Y() -= dy;
                changed = true;
            }
            mUi->mWantsCaptureMouse = true;
        }

        if (active && mInput.mMouseReleased) {
            mUi->mActiveId = 0ULL;
        }

        auto ResolveAxisColor = [&](u64 id, FColor32 base) -> FColor32 {
            if (mUi->mActiveId == id) {
                return mTheme->mGizmoAxisActive;
            }
            if (hoveredId == id) {
                return mTheme->mGizmoAxisHover;
            }
            return base;
        };

        DrawRectFilled(r, mTheme->mGizmoBg);
        DrawRect(r, mTheme->mGizmoBorder, 1.0f);

        const FColor32 xCol  = ResolveAxisColor(idX, mTheme->mGizmoAxisX);
        const FColor32 yCol  = ResolveAxisColor(idY, mTheme->mGizmoAxisY);
        const FColor32 xyCol = ResolveAxisColor(idXY, mTheme->mGizmoAxisXy);
        DrawLine(center, xEnd, xCol, mTheme->mGizmoAxisThickness);
        DrawLine(center, yEnd, yCol, mTheme->mGizmoAxisThickness);
        DrawRectFilled(centerRect, xyCol);
        DrawRect(centerRect, mTheme->mGizmoBorder, 1.0f);

        DrawText(FVector2f(xEnd.X() + 3.0f, xEnd.Y() - 6.0f), xCol, TEXT("X"));
        DrawText(FVector2f(yEnd.X() + 3.0f, yEnd.Y() - 6.0f), yCol, TEXT("Y"));

        FString valueText;
        valueText.Assign(TEXT("x="));
        valueText.AppendNumber(value.X());
        valueText.Append(TEXT(" y="));
        valueText.AppendNumber(value.Y());
        DrawText(FVector2f(r.Min.X(), r.Max.Y() + 3.0f), mTheme->mText, valueText.ToView());

        AdvanceItem(FVector2f(size, size + mTheme->mGizmoBottomSpacingY + 14.0f));
        return changed;
    }
} // namespace AltinaEngine::DebugGui::Private
